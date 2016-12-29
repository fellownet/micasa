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
		return v7_throwf( js_, "Error", "Invalid parameter." );
	}
	int deviceId = v7_get_int( js_, arg0 );
	auto device = controller->_getDeviceById( deviceId );
	if ( device == nullptr ) {
		return v7_throwf( js_, "Error", "Invalid device." );
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
			return v7_throwf( js_, "Error", "Invalid parameter for device." );
		}
	} else if ( v7_is_string( arg1 ) ) {
		std::string value = v7_get_string( js_, &arg1, NULL );
		if ( device->getType() == micasa::Device::Type::SWITCH ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Switch>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::TEXT ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Text>( device ), value, options ) );
		} else {
			return v7_throwf( js_, "Error", "Invalid parameter for device." );
		}
	} else {
		return v7_throwf( js_, "Error", "Invalid parameter for device." );
	}

	return V7_OK;
};

v7_err micasa_v7_get_device( struct v7* js_, v7_val_t* res_ ) {
	micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );

	std::shared_ptr<micasa::Device> device = nullptr;
	
	v7_val_t arg0 = v7_arg( js_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->_getDeviceById( v7_get_int( js_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->_getDeviceByLabel( v7_get_string( js_, &arg0, NULL ) );
	}
	if ( device == nullptr ) {
		return v7_throwf( js_, "Error", "Invalid device." );
	}
	
	v7_err js_error;
	switch( device->getType() ) {
		case micasa::Device::Type::SWITCH:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Switch>( device )->getJson().dump().c_str(), res_ );
			break;
		case micasa::Device::Type::TEXT:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Text>( device )->getJson().dump().c_str(), res_ );
			break;
		case micasa::Device::Type::LEVEL:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Level>( device )->getJson().dump().c_str(), res_ );
			break;
		case micasa::Device::Type::COUNTER:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Counter>( device )->getJson().dump().c_str(), res_ );
			break;
	}

	if ( V7_OK != js_error ) {
		return v7_throwf( js_, "Error", "Internal error." );
	}
	
	return V7_OK;
};

namespace micasa {

	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	using namespace std::chrono;

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
		this->m_settings.populate();
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
		
		// Fetch all the hardware from the database to initialize our local map of hardware instances.
		std::unique_lock<std::mutex> hardwareLock( this->m_hardwareMutex );
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
		hardwareLock.unlock();

		// Install the script- and cron resource handlers.
		this->_updateScriptResourceHandlers();
		this->_updateCronResourceHandlers();
	
		// Install resource to receive version number. This resource is also used by the client to determine if
		// the server is available at the current url (hence being served by micasa or a 3rd party webserver).
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller",
			"api/controller",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				// TODO introduce some sort of central place where the version number comes from
				output_["version"] = "0.0.1";
			} )
		} ) ) );
		
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		
		g_webServer->removeResourceCallback( "controller" );
		g_webServer->removeResourceCallback( "controller-scripts" );
		g_webServer->removeResourceCallback( "controller-crons" );
		
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
		//assert( this->isRunning() && "Controller should be running before declaring hardware." );
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

		
		Settings& settings = hardware->getSettings();
		settings.insert( settings_ );
		settings.commit( *hardware );
		
		hardware->start();

		// Reacquire lock when adding to the map to make it thread safe.
		lock = std::unique_lock<std::mutex>( this->m_hardwareMutex );
		this->m_hardware.push_back( hardware );
		lock.unlock();
		
		return hardware;
	};
	
	void Controller::removeHardware( std::shared_ptr<Hardware> hardware_ ) {
		std::unique_lock<std::mutex> lock( this->m_hardwareMutex );

		if ( hardware_->isRunning() ) {
			hardware_->stop();
		}
		
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt) == hardware_ ) {
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
			// TODO Only fetch those scripts that are present in the script / device crosstable.
			this->_runScripts( "event", event, g_database->getQuery(
				"SELECT s.`id`, s.`name`, s.`code`, s.`status`, s.`runs` "
				"FROM `scripts` s, `x_device_scripts` x "
				"WHERE x.`script_id`=s.`id` "
				"AND x.`device_id`=%d "
				"AND s.`status`=1 "
				"ORDER BY `id` ASC",
				device_.getId()
			) );
		}
	};
	template void Controller::newEvent( const Switch& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Level& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Counter& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Text& device_, const unsigned int& source_ );
	
	void Controller::_runScripts( const std::string& key_, const json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ ) {
		// Event processing is done in a separate thread to prevent scripts from blocking hardare updates.
		// TODO insert a task that checks if the script isn't running for more than xx seconds? This requires that
		// we don't detach and keep track of the thread.
		auto thread = std::thread( [this,key_,data_,scripts_]{
			std::lock_guard<std::mutex> lock( this->m_scriptsMutex );
			
			// Create v7 environment.
			// TODO make a controller property out of this and re-use it across events?
			v7* js = v7_create();
			v7_set_user_data( js, v7_get_global( js ), this );
			
			v7_val_t dataObj;
			if ( V7_OK != v7_parse_json( js, data_.dump().c_str(), &dataObj ) ) {
				return; // should not happen, but function has attribute warn_unused_result
			}

			v7_val_t userDataObj;
			if ( V7_OK != v7_parse_json( js, this->m_settings.get<std::string>( "userdata", "{}" ).c_str(), &userDataObj ) ) {
				return; // should not happen, but function has attribute warn_unused_result
			}
			
			// Set data and userdata, and install javascript hooks.
			v7_val_t root = v7_get_global( js );
			v7_set( js, root, key_.c_str(), ~0, dataObj );
			v7_set( js, root, "userData", ~0, userDataObj );
			v7_set_method( js, root, "updateDevice", &micasa_v7_update_device );
			v7_set_method( js, root, "getDevice", &micasa_v7_get_device );

			for ( auto scriptsIt = scripts_.begin(); scriptsIt != scripts_.end(); scriptsIt++ ) {
				
				v7_val_t js_result;
				v7_err js_error = v7_exec( js, (*scriptsIt).at( "code" ).c_str(), &js_result );

				unsigned int status = 1;
				
				switch( js_error ) {
					case V7_SYNTAX_ERROR:
						g_logger->logr( Logger::LogLevel::ERROR, this, "Syntax error in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						status = 3; // TODO make an enum out of this
						break;
					case V7_EXEC_EXCEPTION:
						// TODO this exception probably has a message, store and report for easier debugging.
						
						char buf[100], *p;
						p = v7_stringify( js, js_result, buf, sizeof(buf), V7_STRINGIFY_DEFAULT );
						g_logger->logr( Logger::LogLevel::ERROR, this, "Exception in in \"%s\" (%s).", (*scriptsIt).at( "name" ).c_str(), p );
						if (p != buf) {
							free(p);
						}

						status = 3; // TODO make an enum out of this
						break;
					case V7_AST_TOO_LARGE:
					case V7_INTERNAL_ERROR:
						g_logger->logr( Logger::LogLevel::ERROR, this, "Internal error in in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						status = 3; // TODO make an enum out of this
						break;
					case V7_OK:
						g_logger->logr( Logger::LogLevel::VERBOSE, this, "Script \"%s\" executed.", (*scriptsIt).at( "name" ).c_str() );
						break;
				}
				
				g_database->putQuery(
					"UPDATE `scripts` "
					"SET `runs`=`runs`+1, `status`=%d "
					"WHERE `id`=%q",
					status,
					(*scriptsIt).at( "id" ).c_str()
				);
				g_webServer->touchResourceCallback( "controller-scripts" );
			}
			
			// Get the userdata from the v7 environment and store it in settings for later use. If the buffer
			// is too small to hold all the data, v7 will allocate more and we need to free it ourselves.
			userDataObj = v7_get( js, root, "userData", ~0 );
			char buffer[1024], *p;
			p = v7_stringify( js, userDataObj, buffer, sizeof( buffer ), V7_STRINGIFY_JSON );
			if ( p != buffer ) {
				free( p );
			}
			
			this->m_settings.put( "userdata", std::string( buffer ) );
			this->m_settings.commit();

			v7_destroy( js );
			
		} );
		thread.detach();
	};

	std::shared_ptr<Device> Controller::_getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceById( id_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};
	
	std::shared_ptr<Device> Controller::_getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceByLabel( label_ );
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
			Task task = { device_, options.recur ? (unsigned int)0 : Device::UpdateSource::SCRIPT, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
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
				Task task = { device_, options.recur ? (unsigned int)0 : Device::UpdateSource::SCRIPT, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
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

	const milliseconds Controller::_work( const unsigned long int& iteration_ ) {
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );

		auto now = system_clock::now();
		static unsigned int minuteSinceEpoch = duration_cast<minutes>( now.time_since_epoch() ).count();
		if ( minuteSinceEpoch < duration_cast<minutes>( now.time_since_epoch() ).count() ) {
#ifdef _DEBUG
			g_logger->log( Logger::LogLevel::DEBUG, this, "Running crons." );
#endif // _DEBUG

			std::lock_guard<std::mutex> lock( this->m_cronsMutex );
			minuteSinceEpoch = duration_cast<minutes>( now.time_since_epoch() ).count();

			auto crons = g_database->getQuery(
				"SELECT `id`, `cron`, `name` "
				"FROM `crons` "
				"WHERE `enabled`=1 "
				"ORDER BY `id` ASC"
			);
			for ( auto cronIt = crons.begin(); cronIt != crons.end(); cronIt++ ) {
			
				try { // invalid crons throw std::runtime_error exceptions
			
					bool run = true;
			
					// TODO regexp the entire cron for added security? client side has the regexp in place.
					// TODO get inspired by https://github.com/davidmoreno/garlic/blob/master/src/cron.cpp
					// TODO take UTC into account. now() on systemclock is in utc > maybe attach user with user timezone?
			
					// First the entiry cron is plit up in parts. There should be exactly 5 parts, m h dom mon dow.
					std::vector<std::string> cronParts;
					stringSplit( (*cronIt)["cron"], " ", cronParts );
					if ( cronParts.size() != 5 ) {
						throw std::runtime_error( "invalid number of cron parts" );
						continue;
					}
					for ( unsigned int index = 0; index <= 4; index++ ) {
						std::string cronPart = cronParts[index];
					
						// Then a list is processed. A single entry is wrapped into a list for convenience. Then each
						// entry in the list is processed further.
						std::vector<std::string> list;
						if ( cronPart.find( "," ) != std::string::npos ) {
							stringSplit( cronPart, ",", list );
						} else {
							list.push_back( cronPart );
						}
						for ( auto listIt = list.begin(); listIt != list.end(); listIt++ ) {
							std::string entry = (*listIt);
						
							// Then we're looking for every other x style forward-slashes. These act like a modulo, so
							// store it in the modulo var.
							unsigned int modulo = 60;
							if ( entry.find( "/" ) != std::string::npos ) {
								std::vector<std::string> entryParts;
								stringSplit( entry, "/", entryParts );
								if ( entryParts.size() != 2 ) {
									throw std::runtime_error( "invalid cron interval syntax" );
								}
								modulo = std::stoi( entryParts[1] );
								if ( modulo == 0 ) {
									throw std::runtime_error( "invalid cron interval" );
								}
								entry = entryParts[0];
							}
							
							// Then we're looking for ranges in the form of xx-xx.
							unsigned int min;
							unsigned int max;
							if ( entry.find( "-" ) != std::string::npos ) {
								std::vector<std::string> entryParts;
								stringSplit( entry, "-", entryParts );
								if ( entryParts.size() != 2 ) {
									throw std::runtime_error( "invalid cron range syntax" );
								}
								min = std::stoi( entryParts[0] );
								max = std::stoi( entryParts[1] );
							} else {
								if ( entry == "*" ) {
									min = max = minuteSinceEpoch % 60;
								} else {
									min = max = std::stoi( entry );
									if ( min > max ) {
										throw std::runtime_error( "invalid cron range" );
									}
								}
							}

							// Then we're going to determine if the cron should run or not.
							switch( index ) {
								case 0: { // m
									if (
										( minuteSinceEpoch % 60 ) % modulo < min
										|| ( minuteSinceEpoch % 60 ) % modulo > max
									) {
										run = false;
									}
									break;
								}
								case 1: { // h
									/*
									if (
										( minuteSinceEpoch % 1440 ) < min * 60
										|| ( minuteSinceEpoch % 1440 ) > max * 60
									) {
										g_logger->logr( Logger::LogLevel::VERBOSE, this, "NOP 2" );
										run = false;
									}
									*/
									break;
								}
								case 2: { // dom
									break;
								}
								case 3: { // mon
									break;
								}
								case 4: { // dow
									break;
								}
							}
							
							
							
							
							
						}
					}
					

					if ( run ) {
						json data = (*cronIt);
						data["minute"] = minuteSinceEpoch % 60;
						data["hour"] = ( minuteSinceEpoch % 1440 ) / 60;
						this->_runScripts( "cron", data, g_database->getQuery(
							"SELECT s.`id`, s.`name`, s.`code`, s.`status`, s.`runs` "
							"FROM `x_cron_scripts` x, `scripts` s "
							"WHERE x.`script_id`=s.`id` "
							"AND x.`cron_id`=%q "
							"AND s.`status`=1 "
							"ORDER BY s.`id` ASC",
							(*cronIt)["id"].c_str()
						) );
					}
				} catch( const std::exception& exception_ ) {
					g_logger->logr( Logger::LogLevel::ERROR, this, "Invalid cron (%s).", exception_.what() );
				}
			}
		}
		
		// Investigate the front of the queue for tasks that are due. If the next task is not due we're done due to
		// the fact that _scheduleTask keeps the list sorted on scheduled time.
		auto taskQueueIt = this->m_taskQueue.begin();
		while(
			taskQueueIt != this->m_taskQueue.end()
			&& (*taskQueueIt)->scheduled <= now + milliseconds( 5 )
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
		
		// This method should run a least every minute for cron jobs to run. The 5ms is a safe margin to make sure
		// the whole minute has passed.
		auto wait = milliseconds( 60005 ) - duration_cast<milliseconds>( now.time_since_epoch() ) % milliseconds( 60000 );

		// If there's nothing in the queue anymore we can wait the default duration before investigating the queue
		// again. This is because the _scheduleTask method will wake us up if there's work to do anyway.
		if ( taskQueueIt != this->m_taskQueue.end() ) {
			auto delay = duration_cast<milliseconds>( (*taskQueueIt)->scheduled - now );
			if ( delay < wait ) {
				wait = delay;
			}
		}

		return wait;
	};

	const Controller::TaskOptions Controller::_parseTaskOptions( const std::string& options_ ) const {
		int lastTokenType = 0;
		std::vector<std::string> optionParts;
		TaskOptions result = { 0, 0, 1, 0, false, false };

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
			} else if ( part == "RECUR" ) {
				result.recur = true;
				lastTokenType = 0;
			} else if ( part.find( "SECOND" ) != std::string::npos ) {
				// Do nothing, seconds are the default.
				lastTokenType = 0;
			} else if ( part.find( "MINUTE" ) != std::string::npos ) {
				switch( lastTokenType ) {
					case 1: result.forSec *= 60.; break;
					case 2: result.afterSec *= 60.; break;
					case 4: result.repeatSec *= 60.; break;
				}
				lastTokenType = 0;
			} else if ( part.find( "HOUR" ) != std::string::npos ) {
				switch( lastTokenType ) {
					case 1: result.forSec *= 3600.; break;
					case 2: result.afterSec *= 3600.; break;
					case 4: result.repeatSec *= 3600.; break;
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

	void Controller::_updateScriptResourceHandlers() const {

		// First all current callbacks for scripts are removed.
		g_webServer->removeResourceCallback( "controller-scripts" );
		
		// The first callback is the general script fetch callback that lists all available scripts and can
		// be used to add (post) new scripts.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-scripts",
			"api/scripts",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_scriptsMutex );
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = g_database->getQuery<json>(
							"SELECT `id`, `name`, `code`, `status`, `runs` "
							"FROM `scripts` "
							"ORDER BY `id` ASC"
						);
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "name") != input_.end()
								&& input_.find( "code") != input_.end()
							) {
								auto scriptId = g_database->putQuery(
									"INSERT INTO `scripts` (`name`, `code`) "
									"VALUES (%Q, %Q) ",
									input_["name"].get<std::string>().c_str(),
									input_["code"].get<std::string>().c_str()
								);
								this->_updateScriptResourceHandlers();
								auto scriptData = g_database->getQueryRow(
									"SELECT `id`, `name`, `code`, `status`, `runs` "
									"FROM `scripts` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									scriptId
								);
								output_["result"] = "OK";
								output_["script"] = scriptData;
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing parameters.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to save script.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		
		// Then a specific resource handler is installed for each script. This handler allows for retrieval as
		// well as updating a script.
		auto scriptIds = g_database->getQueryColumn<unsigned int>(
			"SELECT `id` "
			"FROM `scripts` "
			"ORDER BY `id` ASC"
		);
		for ( auto scriptIdsIt = scriptIds.begin(); scriptIdsIt != scriptIds.end(); scriptIdsIt++ ) {
			unsigned int scriptId = (*scriptIdsIt);
			g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
				"controller-scripts",
				"api/scripts/" + std::to_string( scriptId ),
				WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
				WebServer::t_callback( [this,scriptId]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
					std::lock_guard<std::mutex> lock( this->m_scriptsMutex );
					switch( method_ ) {
						case WebServer::Method::PUT: {
							try {
								if (
									input_.find( "name") != input_.end()
									&& input_.find( "code") != input_.end()
								) {
									g_database->putQuery(
										"UPDATE `scripts` "
										"SET `name`=%Q, `code`=%Q, `status`=1, `runs`=0 "
										"WHERE `id`=%d",
										input_["name"].get<std::string>().c_str(),
										input_["code"].get<std::string>().c_str(),
										scriptId
									);
									g_webServer->touchResourceCallback( "controller-scripts" );
									auto scriptData = g_database->getQueryRow(
										"SELECT `id`, `name`, `code`, `status`, `runs` "
										"FROM `scripts` "
										"WHERE `id`=%d "
										"ORDER BY `id` ASC",
										scriptId
									);
									output_["result"] = "OK";
									output_["script"] = scriptData;
								} else {
									output_["result"] = "ERROR";
									output_["message"] = "Missing parameters.";
									code_ = 400; // bad request
								}
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save script.";
								code_ = 400; // bad request
							}
							break;
						}
						case WebServer::Method::GET: {
							auto scriptData = g_database->getQueryRow(
								"SELECT `id`, `name`, `code`, `status`, `runs` "
								"FROM `scripts` "
								"WHERE `id`=%d "
								"ORDER BY `id` ASC",
								scriptId
							);
							output_ = scriptData;
							break;
						}
						case WebServer::Method::DELETE: {
							try {
								g_database->putQuery(
									"DELETE FROM `scripts` "
									"WHERE `id`=%d",
									scriptId
								);
								this->_updateScriptResourceHandlers();
								output_["result"] = "OK";
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save script.";
								code_ = 400; // bad request
							}
							break;
						}
						default: break;
					}
				} )
			} ) ) );
		}
	};

	void Controller::_updateCronResourceHandlers() const {

		// First all current callbacks for crons are removed.
		g_webServer->removeResourceCallback( "controller-crons" );

		const auto setScripts = []( const json& scriptIds_, unsigned int cronId_ ) {
			std::stringstream list;
			unsigned int index = 0;
			for ( auto scriptIdsIt = scriptIds_.begin(); scriptIdsIt != scriptIds_.end(); scriptIdsIt++ ) {
				auto scriptId = (*scriptIdsIt);
				if ( scriptId.is_number() ) {
					list << ( index > 0 ? "," : "" ) << scriptId;
					index++;
					g_database->putQuery(
						"INSERT OR IGNORE INTO `x_cron_scripts` "
						"(`cron_id`, `script_id`) "
						"VALUES (%d, %d)",
						cronId_,
						scriptId.get<unsigned int>()
					);
				}
			}
			g_database->putQuery(
				"DELETE FROM `x_cron_scripts` "
				"WHERE `cron_id`=%d "
				"AND `script_id` NOT IN (%q)",
				cronId_,
				list.str().c_str()
			);
		};
		
		// The first callback is the general cron fetch callback that lists all available crons and can
		// be used to add (post) new crons.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-crons",
			"api/crons",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this,&setScripts]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_cronsMutex );
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = json::array();
						auto crons = g_database->getQuery<json>(
							"SELECT `id`, `name`, `cron` "
							"FROM `crons` "
							"ORDER BY `id` ASC"
						);
						for ( auto cronsIt = crons.begin(); cronsIt != crons.end(); cronsIt++ ) {
							json cron = (*cronsIt);
							cron["scripts"] = g_database->getQueryColumn<unsigned int>(
								"SELECT s.`id` "
								"FROM `scripts` s, `x_cron_scripts` x "
								"WHERE s.`id`=x.`script_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY s.`id` ASC",
								(*cronsIt)["id"].get<unsigned int>()
							);
							cron["devices"] = g_database->getQuery<json>(
								"SELECT d.`id`, x.`value` "
								"FROM `devices` d, `x_cron_devices` x "
								"WHERE d.`id`=x.`device_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY d.`id` ASC",
								(*cronsIt)["id"].get<unsigned int>()
							);
							output_ += cron;
						}
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "name") != input_.end()
								&& input_.find( "cron") != input_.end()
							) {
								auto cronId = g_database->putQuery(
									"INSERT INTO `crons` (`name`, `cron`) "
									"VALUES (%Q, %Q) ",
									input_["name"].get<std::string>().c_str(),
									input_["cron"].get<std::string>().c_str()
								);
								if (
									input_.find( "scripts") != input_.end()
									&& input_["scripts"].is_array()
								) {
									setScripts( input_["scripts"], cronId );
								}
								this->_updateCronResourceHandlers();
								auto cronData = g_database->getQueryRow(
									"SELECT `id`, `name`, `cron` "
									"FROM `crons` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									cronId
								);
								output_["result"] = "OK";
								output_["cron"] = cronData;
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing parameters.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to save cron.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		
		// Then a specific resource handler is installed for each cron. This handler allows for retrieval as
		// well as updating a cron.
		auto cronIds = g_database->getQueryColumn<unsigned int>(
			"SELECT `id` "
			"FROM `crons` "
			"ORDER BY `id` ASC"
		);
		for ( auto cronIdsIt = cronIds.begin(); cronIdsIt != cronIds.end(); cronIdsIt++ ) {
			unsigned int cronId = (*cronIdsIt);
			g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
				"controller-crons",
				"api/crons/" + std::to_string( cronId ),
				WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
				WebServer::t_callback( [this,cronId,&setScripts]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
					std::lock_guard<std::mutex> lock( this->m_cronsMutex );
					switch( method_ ) {
						case WebServer::Method::PUT: {
							try {
								if (
									input_.find( "name") != input_.end()
									&& input_.find( "cron") != input_.end()
								) {
									g_database->putQuery(
										"UPDATE `crons` "
										"SET `name`=%Q, `cron`=%Q "
										"WHERE `id`=%d",
										input_["name"].get<std::string>().c_str(),
										input_["cron"].get<std::string>().c_str(),
										cronId
									);
									if (
										input_.find( "scripts") != input_.end()
										&& input_["scripts"].is_array()
									) {
										setScripts( input_["scripts"], cronId );
									}
									g_webServer->touchResourceCallback( "controller-crons" );
									auto cronData = g_database->getQueryRow(
										"SELECT `id`, `name`, `cron` "
										"FROM `crons` "
										"WHERE `id`=%d "
										"ORDER BY `id` ASC",
										cronId
									);
									output_["result"] = "OK";
									output_["cron"] = cronData;
								} else {
									output_["result"] = "ERROR";
									output_["message"] = "Missing parameters.";
									code_ = 400; // bad request
								}
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save cron.";
								code_ = 400; // bad request
							}
							break;
						}
						case WebServer::Method::GET: {
							auto cronData = g_database->getQueryRow(
								"SELECT `id`, `name`, `cron` "
								"FROM `crons` "
								"WHERE `id`=%d "
								"ORDER BY `id` ASC",
								cronId
							);
							output_ = cronData;
							output_["scripts"] = g_database->getQueryColumn<unsigned int>(
								"SELECT s.`id` "
								"FROM `scripts` s, `x_cron_scripts` x "
								"WHERE s.`id`=x.`script_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY s.`id` ASC",
								cronId
							);
							output_["devices"] = g_database->getQuery<json>(
								"SELECT d.`id`, x.`value` "
								"FROM `devices` d, `x_cron_devices` x "
								"WHERE d.`id`=x.`device_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY d.`id` ASC",
								cronId
							);
							break;
						}
						case WebServer::Method::DELETE: {
							try {
								g_database->putQuery(
									"DELETE FROM `crons` "
									"WHERE `id`=%d",
									cronId
								);
								this->_updateCronResourceHandlers();
								output_["result"] = "OK";
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save cron.";
								code_ = 400; // bad request
							}
							break;
						}
						default: break;
					}
				} )
			} ) ) );
		}
	};

}; // namespace micasa
