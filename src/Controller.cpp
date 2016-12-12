// v7
// https://docs.cesanta.com/v7/master/

#include <iostream>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <dirent.h>

#include "Controller.h"
#include "Database.h"
#include "Utils.h"

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
	auto device = controller->_getDeviceById( deviceId );
	if ( device == nullptr ) {
		return V7_EXEC_EXCEPTION;
	}
	
	v7_val_t arg1 = v7_arg( js_, 1 );
	
	std::string options = "";
	v7_val_t arg2 = v7_arg( js_, 2 );
	if ( v7_is_string( arg2 ) ) {
		options = v7_get_string( js_, &arg2, NULL );
	}

	if ( v7_is_number( arg1 ) ) {
		double value = v7_get_double( js_, arg1 );
		if ( device->getType() == micasa::Device::Type::COUNTER ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Counter>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::LEVEL ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Level>( device ), value, options ) );
		} else {
			return V7_EXEC_EXCEPTION;
		}
	} else if ( v7_is_string( arg1 ) ) {
		std::string value = v7_get_string( js_, &arg1, NULL );
		if ( device->getType() == micasa::Device::Type::SWITCH ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Switch>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::TEXT ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Text>( device ), value, options ) );
		} else {
			return V7_EXEC_EXCEPTION;
		}
	} else {
		return V7_EXEC_EXCEPTION;
	}

	return V7_OK;
};

v7_err micasa_v7_get_device( struct v7* js_, v7_val_t* res_ ) {
	//micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );

	v7_val_t arg0 = v7_arg( js_, 0 );
	if ( v7_is_number( arg0 ) ) {
		
		
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

	template<> void Controller::Task::setValue<Level>( const typename Level::t_value& value_ ) { this->levelValue = value_; };
	template<> void Controller::Task::setValue<Counter>( const typename Counter::t_value& value_ ) { this->counterValue = value_; };
	template<> void Controller::Task::setValue<Switch>( const typename Switch::t_value& value_ ) { this->switchValue = value_; };
	template<> void Controller::Task::setValue<Text>( const typename Text::t_value& value_ ) { this->textValue = value_; };
	
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
			"SELECT `id`, `hardware_id`, `reference`, `label`, `type` "
			"FROM `hardware` "
			"WHERE `enabled`=1 "
			"ORDER BY `id` ASC" // child hardware should come *after* parents
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
			
			Hardware::Type type = static_cast<Hardware::Type>( atoi( (*hardwareIt)["type"].c_str() ) );
			std::shared_ptr<Hardware> hardware = Hardware::_factory( type, std::stoi( (*hardwareIt)["id"] ), (*hardwareIt)["reference"], parent, (*hardwareIt)["label"] );
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

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_ ) {
		return this->declareHardware( type_, reference_, nullptr, label_, settings_ );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::string label_, const std::map<std::string, std::string> settings_ ) {
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
				"INSERT INTO `hardware` ( `hardware_id`, `reference`, `type`, `label` ) "
				"VALUES ( %d, %Q, %d, %Q )"
				, parent_->getId(), reference_.c_str(), static_cast<int>( type_ ), label_.c_str()
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `reference`, `type`, `label` ) "
				"VALUES ( %Q, %d, %Q )"
				, reference_.c_str(), static_cast<int>( type_ ), label_.c_str()
			);
		}
		
		// The lock is released while instantiating hardware because some hardware need to lookup their parent with
		// the getHardware* methods which also use the lock.
		lock.unlock();
		std::shared_ptr<Hardware> hardware = Hardware::_factory( type_, id, reference_, parent_, label_ );
		//lock.lock();
		
		Settings& settings = hardware->getSettings();
		settings.insert( settings_ );
		settings.commit( *hardware );
		
		hardware->start();
		this->m_hardware.push_back( hardware );
		
		return hardware;
	};
	
	void Controller::removeHardware( std::shared_ptr<Hardware> hardware_ ) {
		std::unique_lock<std::mutex> lock( this->m_hardwareMutex );

		if ( hardware_->isRunning() ) {
			hardware_->stop();
		}
		
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt) == hardware_ ) {
				std::cout << "GEVONDEN JAJA\n";
				this->m_hardware.erase( hardwareIt );
				break;
			}
		}
		
		// The query below should also remove all related devices and settings due to foreign keys being active.
		g_database->putQuery(
			"DELETE FROM `hardware` "
			"WHERE `id`=%d"
			, hardware_->getId()
		);
	};
	
	template<class D> void Controller::newEvent( const D& device_, const unsigned int& source_ ) {
		// Events originating from scripts should not cause another script run.
		// TODO make this configurable per device?
		if (
			( source_ & Device::UpdateSource::SCRIPT ) != Device::UpdateSource::SCRIPT
			&& ( source_ & Device::UpdateSource::INIT ) != Device::UpdateSource::INIT
		) {
			json event;
			event["value"] = device_.getValue();
			event["source"] = source_;
			event["device"] = device_.getJson();
			event["hardware"] = device_.getHardware()->getJson();
			
			// The processing of the event is deliberatly done in a separate method because this method is templated
			// and is essentially copied for each specialization.
			this->_processEvent( event );
		}
	};
	template void Controller::newEvent( const Switch& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Level& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Counter& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Text& device_, const unsigned int& source_ );
	
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
						
						v7* js = v7_create();
						v7_set_user_data( js, v7_get_global( js ), this );
						
						// Install javascript hooks.
						v7_set_method( js, v7_get_global( js ), "updateDevice", &micasa_v7_update_device );
						v7_set_method( js, v7_get_global( js ), "getDevice", &micasa_v7_get_device );
						
						v7_val_t js_result;
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
		thread.detach();
	};

	const std::shared_ptr<Device> Controller::_getDeviceById( const unsigned int id_ ) const {
		// TODO iterating through both the hardware and device maps seems a bit heavy, can this be done more efficiently?
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceById( id_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};
	
	template<class D> bool Controller::_updateDeviceFromScript( const std::shared_ptr<D>& device_, const typename D::t_value& value_, const std::string& options_ ) {
		TaskOptions options = this->_parseTaskOptions( options_ );

		if ( options.clear ) {
			this->_clearTaskQueue( device_ );
		}
		
		typename D::t_value previousValue = device_->getValue();
		for ( int i = 0; i < abs( options.repeat ); i++ ) {
			double delaySec = options.afterSec + ( i * options.forSec ) + ( i * options.repeatSec );
			
			// TODO implement "recur" task option > if set do not set source to script so that the result of the
			// task will also be executed by scripts. There needs to be some detection though to prevent a loop.
			Task task = { device_, Device::UpdateSource::SCRIPT, std::chrono::system_clock::now() + std::chrono::milliseconds( (int)( delaySec * 1000 ) ) };
			task.setValue<D>( value_ );
			this->_scheduleTask( std::make_shared<Task>( task ) );
			
			if (
				options.forSec > 0.05
				&& (
					options.repeat > 0
					|| i < abs( options.repeat ) - 1
				)
			) {
				delaySec += options.forSec;
				Task task = { device_, Device::UpdateSource::SCRIPT, std::chrono::system_clock::now() + std::chrono::milliseconds( (int)( delaySec * 1000 ) ) };
				task.setValue<D>( previousValue );
				this->_scheduleTask( std::make_shared<Task>( task ) );
			}
		}
		return true;
	};
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Level>& device_, const typename Level::t_value& value_, const std::string& options_ );
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Counter>& device_, const typename Counter::t_value& value_, const std::string& options_ );
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Switch>& device_, const typename Switch::t_value& value_, const std::string& options_ );
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Text>& device_, const typename Text::t_value& value_, const std::string& options_ );
	
	void Controller::_clearTaskQueue( const std::shared_ptr<Device>& device_ ) {
		std::unique_lock<std::mutex> lock( this->m_taskQueueMutex );
		for ( auto taskQueueIt = this->m_taskQueue.begin(); taskQueueIt != this->m_taskQueue.end(); ) {
			// TODO check if comparing devices directly also works instead of their ids.
			if ( (*taskQueueIt)->device->getId() == device_->getId() ) {
				taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
			} else {
				taskQueueIt++;
			}
		}
	};
	
	void Controller::_scheduleTask( const std::shared_ptr<Task> task_ ) {
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

	const std::chrono::milliseconds Controller::_work( const unsigned long int& iteration_ ) {
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
		
		// Investigate the front of the queue for tasks that are due. If the next task is not due we're done due to
		// the fact that _scheduleTask keeps the list sorted on scheduled time.
		auto taskQueueIt = this->m_taskQueue.begin();
		auto now = std::chrono::system_clock::now();
		while(
			taskQueueIt != this->m_taskQueue.end()
			&& (*taskQueueIt)->scheduled <= now + std::chrono::milliseconds( 5 )
		) {
			std::shared_ptr<Task> task = (*taskQueueIt);
			
			switch( task->device->getType() ) {
				case Device::Type::COUNTER:
					std::static_pointer_cast<Counter>( task->device )->updateValue( task->source, task->counterValue );
					break;
				case Device::Type::LEVEL:
					std::static_pointer_cast<Level>( task->device )->updateValue( task->source, task->levelValue );
					break;
				case Device::Type::SWITCH:
					std::static_pointer_cast<Switch>( task->device )->updateValue( task->source, task->switchValue );
					break;
				case Device::Type::TEXT:
					std::static_pointer_cast<Text>( task->device )->updateValue( task->source, task->textValue );
					break;
			}
			taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
		}
		
		// If there's nothing in the queue anymore we can wait a 'long' time before investigating the queue again. This
		// is because the _scheduleTask method will wake us up if there's work to do anyway.
		// TODO wake up every minute to execute timed scripts?
		auto wait = std::chrono::milliseconds( 15000 );
		if ( taskQueueIt != this->m_taskQueue.end() ) {
			auto delay = (*taskQueueIt)->scheduled - now;
			wait = std::chrono::duration_cast<std::chrono::milliseconds>( delay );
		}
		
		return std::chrono::milliseconds( wait );
	};

	const Controller::TaskOptions Controller::_parseTaskOptions( const std::string& options_ ) const {
		int lastTokenType = 0;
		std::vector<std::string> optionParts;
		TaskOptions result = { 0, 0, 1, 0, false };

		stringSplit( options_, " ", optionParts );
		for ( auto partsIt = optionParts.begin(); partsIt != optionParts.end(); ++partsIt ) {
			std::string part = *partsIt;
			std::transform( part.begin(), part.end(), part.begin(), ::toupper );
			
			if ( part == "FOR" ) {
				lastTokenType = 1;
			} else if ( part == "AFTER" ) {
				lastTokenType = 2;
			} else if ( part == "REPEAT" ) {
				lastTokenType = 3;
			} else if ( part == "INTERVAL" ) {
				lastTokenType = 4;
			} else if ( part == "CLEAR" ) {
				result.clear = true;
				lastTokenType = 0;
			} else if ( part.find( "SECOND" ) != std::string::npos ) {
				// Do nothing, seconds are the default.
				lastTokenType = 0;
			} else if ( part.find( "MINUTE" ) != std::string::npos ) {
				switch( lastTokenType ) {
					case 1: result.forSec *= 60.; break;
					case 2: result.afterSec *= 60.; break;
					case 3: result.repeatSec *= 60.; break;
				}
				lastTokenType = 0;
			} else if ( part.find( "HOUR" ) != std::string::npos ) {
				switch( lastTokenType ) {
					case 1: result.forSec *= 3600.; break;
					case 2: result.afterSec *= 3600.; break;
					case 3: result.repeatSec *= 3600.; break;
				}
				lastTokenType = 0;
			} else {
				switch( lastTokenType ) {
					case 1:
						result.forSec = std::stod( part );
						break;
					case 2:
						result.afterSec = std::stod( part );
						break;
					case 3:
						result.repeat = std::stoi( part );
						break;
					case 4:
						result.repeatSec = std::stod( part );
						break;
				}
			}
		}
		
		return result;
	};
	
}; // namespace micasa
