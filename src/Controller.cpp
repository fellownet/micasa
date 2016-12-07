// v7
// https://docs.cesanta.com/v7/master/

#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <dirent.h>

#include "Controller.h"
#include "Database.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

v7_err micasa_v7_update_device( struct v7* js_, v7_val_t* res_ ) {
	micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );

	v7_val_t arg0 = v7_arg( js_, 0 );
	if ( ! v7_is_number( arg0 ) ) {
		return V7_EXEC_EXCEPTION;
	}
	int deviceId = v7_get_int( js_, arg0 );
	
	v7_val_t arg1 = v7_arg( js_, 1 );
	if ( v7_is_number( arg1 ) ) {
		double value = v7_get_double( js_, arg1 );
		*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( deviceId, value ) );
	} else if ( v7_is_string( arg1 ) ) {
		std::string value = v7_get_string( js_, &arg1, NULL );
		*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( deviceId, value ) );
	} else {
		return V7_EXEC_EXCEPTION;
	}

	return V7_OK;
};

namespace micasa {

	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;

	Controller::Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before global Controller instance." );
		assert( g_database && "Global Database instance should be created before global Controller instance." );
		assert( g_logger && "Global Logger instance should be created before global Controller instance." );
#endif // _DEBUG
	};

	Controller::~Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after global Controller instance." );
		assert( g_database && "Global Database instance should be destroyed after global Controller instance." );
		assert( g_logger && "Global Logger instance should be destroyed after global Controller instance." );
#endif // _DEBUG
	};

	void Controller::start() {
#ifdef _DEBUG
		assert( g_webServer->isRunning() && "WebServer should be running before Controller is started." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );

		std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery(
			"SELECT `id`, `hardware_id`, `reference`, `name`, `type` "
			"FROM `hardware` "
			"WHERE `enabled`=1 "
			"ORDER BY `id` ASC " // child hardware should come *after* parents
		);
		for ( auto hardwareIt = hardwareData.begin(); hardwareIt != hardwareData.end(); hardwareIt++ ) {
			
			unsigned int parentId = atoi( (*hardwareIt)["hardware_id"].c_str() );
			std::shared_ptr<Hardware> parent = nullptr;
			if ( parentId > 0 ) {
				for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
					if ( (*hardwareIt)->getId() == parentId ) {
						parent = (*hardwareIt);
						break;
					}
				}
			}
			
			Hardware::HardwareType hardwareType = static_cast<Hardware::HardwareType>( atoi( (*hardwareIt)["type"].c_str() ) );
			std::shared_ptr<Hardware> hardware = Hardware::_factory( hardwareType, std::stoi( (*hardwareIt)["id"] ), (*hardwareIt)["reference"], parent, (*hardwareIt)["name"] );
			hardware->start();
			this->m_hardware.push_back( hardware );
		}
		
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Worker::stop();

		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				(*hardwareIt)->stop();
			}
			this->m_hardware.clear();
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Hardware> Controller::getHardware( const std::string reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				return *hardwareIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::HardwareType hardwareType_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ ) {
		return this->declareHardware( hardwareType_, reference_, nullptr, name_, settings_ );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::HardwareType hardwareType_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::string name_, const std::map<std::string, std::string> settings_ ) {
#ifdef _DEBUG
		assert( this->isRunning() && "Controller should be running before declaring hardware." );
		assert( g_webServer->isRunning() && "WebServer should be running before declaring hardware." );
#endif // _DEBUG
		
		std::unique_lock<std::mutex> lock( this->m_hardwareMutex );
		
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				return *hardwareIt;
			}
		}

		long id;
		if ( parent_ ) {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `hardware_id`, `reference`, `type`, `name` ) "
				"VALUES ( %d, %Q, %d, %Q )"
				, parent_->getId(), reference_.c_str(), static_cast<int>( hardwareType_ ), name_.c_str()
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `reference`, `type`, `name` ) "
				"VALUES ( %Q, %d, %Q )"
				, reference_.c_str(), static_cast<int>( hardwareType_ ), name_.c_str()
			);
		}
		
		// The lock is released while instantiating hardware because some hardware need to lookup their parent with
		// the getHardware* methods which also use the lock.
		lock.unlock();
		std::shared_ptr<Hardware> hardware = Hardware::_factory( hardwareType_, id, reference_, parent_, name_ );
		//lock.lock();
		
		Settings& settings = hardware->getSettings();
		settings.insert( settings_ );
		settings.commit( *hardware );
		
		hardware->start();
		this->m_hardware.push_back( hardware );
		
		return hardware;
	};
	
	void Controller::addTask( const std::shared_ptr<Task> task_ ) {
		std::unique_lock<std::mutex> lock( this->m_taskQueueMutex );
		
		// The task is inserted in the list in order of scheduled time. This way the worker method only needs to
		// investigate the front of the queue.
		auto taskIt = this->m_taskQueue.begin();
		while(
			taskIt != this->m_taskQueue.end()
			&& task_->scheduled > (*taskIt)->scheduled
		) {
			taskIt++;
		}
		this->m_taskQueue.insert( taskIt, task_ );
		lock.unlock();
		
		// Immediately wake up the worker to have it start processing scheduled items.
		this->wakeUp();
	};
	
	template<class D> void Controller::newEvent( const D& device_, const Device::UpdateSource& source_ ) {
		// Events originating from scripts should not cause another script run.
		// TODO make this configurable per device?
		if (
			source_ != Device::UpdateSource::SCRIPT
			&& source_ != Device::UpdateSource::INIT
		) {
			json event;
			// TODO for switches this sets the value to an ackward numeric value, use strings instead?
			event["value"] = device_.getValue();
			event["source"] = source_;

			event["device"] = {
				{ "id", device_.getId() },
				{ "name", device_.getName() },
				{ "type", device_.getType() }
			};
			event["hardware"] = {
				{ "id", device_.getHardware()->getId() },
				{ "name", device_.getHardware()->getName() },
			};
			
			// The processing of the event is deliberatly done in a separate method because this method is templated
			// and is essentially copied for each specialization.
			this->_processEvent( event );
		}
	};
	template void Controller::newEvent( const Switch& device_, const Device::UpdateSource& source_ );
	template void Controller::newEvent( const Level& device_, const Device::UpdateSource& source_ );
	template void Controller::newEvent( const Counter& device_, const Device::UpdateSource& source_ );
	template void Controller::newEvent( const Text& device_, const Device::UpdateSource& source_ );
	
	void Controller::_processEvent( const json& event_ ) {
		// Event processing is done in a separate thread to prevent scripts from blocking hardare updates.
		// TODO insert a task that checks if the script isn't running for more than xx seconds? This requires that
		// we don't detach and keep track of the thread.
		auto thread = std::thread( [this,event_]{
			
			// TODO make the var/scripts folder comfigurable?
			// TODO make this cross platform?
			// TODO cache the listing of files?
			// TODO configure the scripts to execute per device instead of all scripts for all device events
			auto directory = opendir( "./var/scripts" );
			while( auto entry = readdir( directory ) ) {
				if ( entry->d_type == DT_REG ) {
					std::string filename = entry->d_name;
					std::string extension = ".js";
					
					if (
						extension.size() < filename.size()
						&& std::equal( extension.rbegin(), extension.rend(), filename.rbegin() )
					) {
						
						std::ifstream filestream( "./var/scripts/" + filename );
						std::string script( ( std::istreambuf_iterator<char>( filestream ) ), ( std::istreambuf_iterator<char>() ) );
						
						script = "event = " + event_.dump() + "; " + script;
						
						v7_val_t js_result;
						v7* js = v7_create();
						
						v7_set_method( js, v7_get_global( js ), "updateDevice", &micasa_v7_update_device );
						v7_set_user_data( js, v7_get_global( js ), this );
						
						v7_err js_error = v7_exec( js, script.c_str(), &js_result );

						switch( js_error ) {
							case V7_SYNTAX_ERROR:
								g_logger->logr( Logger::LogLevel::ERROR, this, "Syntax error in %s.", filename.c_str() );
								break;
							case V7_EXEC_EXCEPTION:
								g_logger->logr( Logger::LogLevel::ERROR, this, "Exception in in %s.", filename.c_str() );
								break;
							case V7_AST_TOO_LARGE:
							case V7_INTERNAL_ERROR:
								g_logger->logr( Logger::LogLevel::ERROR, this, "Internal error in in %s.", filename.c_str() );
								break;
							case V7_OK:
								g_logger->logr( Logger::LogLevel::VERBOSE, this, "Script %s executed.", filename.c_str() );
								break;
						}

						v7_destroy( js );
					}
				}
			}

			closedir( directory );

		} );
		thread.join();
	}

	const bool Controller::_updateDeviceFromScript( const unsigned int& deviceId_, const std::string& value_ ) {
		auto device = this->_getDeviceById( deviceId_ );
		if ( device != nullptr ) {
			// TODO optionally convert strings to floats or ints and accept those too?
			// TODO use the task manager even for tasks that should be executed immediately
			if ( device->getType() == Device::DeviceType::TEXT ) {
				std::static_pointer_cast<Text>( device )->updateValue( Device::UpdateSource::SCRIPT, value_ );
				return true;
			} else if ( device->getType() == Device::DeviceType::SWITCH ) {
				std::static_pointer_cast<Switch>( device )->updateValue( Device::UpdateSource::SCRIPT, value_ );
				return true;
			}
		}
		g_logger->log( Logger::LogLevel::ERROR, this, "Invalid value for device." );
		return false;
	};
	
	const bool Controller::_updateDeviceFromScript( const unsigned int& deviceId_, const double& value_ ) {
		auto device = this->_getDeviceById( deviceId_ );
		if ( device != nullptr ) {
			// TODO optionally convert value to strings and accept those too?
			// TODO use the task manager even for tasks that should be executed immediately
			if ( device->getType() == Device::DeviceType::COUNTER ) {
				return std::static_pointer_cast<Counter>( device )->updateValue( Device::UpdateSource::SCRIPT, value_ );
			} else if ( device->getType() == Device::DeviceType::LEVEL ) {
				return std::static_pointer_cast<Level>( device )->updateValue( Device::UpdateSource::SCRIPT, value_ );
			}
		}
		g_logger->log( Logger::LogLevel::ERROR, this, "Invalid value for device." );
		return false;
	};
	
	std::shared_ptr<Device> Controller::_getDeviceById( const unsigned int id_ ) const {
		// TODO iterating through both the hardware and device maps seems a bit heavy, can this be done more efficiently?
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceById( id_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	}
	
	std::chrono::milliseconds Controller::_work( const unsigned long int iteration_ ) {
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
		
		// Investigate the front of the queue for tasks that are due. If the next task is not due we're done due to
		// the fact that addTask keeps the list sorted on scheduled time.
		auto taskQueueIt = this->m_taskQueue.begin();
		auto now = std::chrono::system_clock::now();
		while(
			taskQueueIt != this->m_taskQueue.end()
			&& (*taskQueueIt)->scheduled <= now + std::chrono::milliseconds( 5 )
		) {
			std::shared_ptr<Task> task = (*taskQueueIt);
			
			g_logger->log( Logger::LogLevel::VERBOSE, this, "TASK" );
			
			taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
		}
		
		// If there's nothing in the queue anymore we can wait a 'long' time before investigating the queue again. This
		// is because the addTask method will wake us up if there's work to do anyway.
		auto wait = std::chrono::milliseconds( 15000 );
		if ( taskQueueIt != this->m_taskQueue.end() ) {
			auto delay = (*taskQueueIt)->scheduled - now;
			wait = std::chrono::duration_cast<std::chrono::milliseconds>( delay );
		}
		
		return std::chrono::milliseconds( wait );
	};
	
}; // namespace micasa
