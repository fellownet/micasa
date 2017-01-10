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

	std::shared_ptr<micasa::Device> device = nullptr;

	v7_val_t arg0 = v7_arg( js_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->getDeviceById( v7_get_int( js_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->getDeviceByLabel( v7_get_string( js_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->getDeviceByName( v7_get_string( js_, &arg0, NULL ) );
		}
	}
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
		device = controller->getDeviceById( v7_get_int( js_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->getDeviceByLabel( v7_get_string( js_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->getDeviceByName( v7_get_string( js_, &arg0, NULL ) );
		}
	}
	if ( device == nullptr ) {
		return v7_throwf( js_, "Error", "Invalid device." );
	}

	v7_err js_error;
	switch( device->getType() ) {
		case micasa::Device::Type::SWITCH:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Switch>( device )->getJson( false ).dump().c_str(), res_ );
			break;
		case micasa::Device::Type::TEXT:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Text>( device )->getJson( false ).dump().c_str(), res_ );
			break;
		case micasa::Device::Type::LEVEL:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Level>( device )->getJson( false ).dump().c_str(), res_ );
			break;
		case micasa::Device::Type::COUNTER:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Counter>( device )->getJson( false ).dump().c_str(), res_ );
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

		// Initialize the v7 javascript environment.
		this->m_v7_js = v7_create();
		v7_val_t root = v7_get_global( this->m_v7_js );
		v7_set_user_data( this->m_v7_js, root, this );

		v7_val_t userDataObj;
		if ( V7_OK != v7_parse_json( this->m_v7_js, this->m_settings.get( "userdata", "{}" ).c_str(), &userDataObj ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in userdata." );
		}
		if ( V7_OK != v7_parse_json( this->m_v7_js, "{}", &userDataObj ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in default userdata." );
		}
		v7_set( this->m_v7_js, root, "userdata", ~0, userDataObj );

		v7_set_method( this->m_v7_js, root, "updateDevice", &micasa_v7_update_device );
		v7_set_method( this->m_v7_js, root, "getDevice", &micasa_v7_get_device );
	};

	Controller::~Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after global Controller instance." );
		assert( g_database && "Global Database instance should be destroyed after global Controller instance." );
		assert( g_logger && "Global Logger instance should be destroyed after global Controller instance." );
#ifdef _WITH_LIBUDEV
		assert( this->m_serialPortCallbacks.size() == 0 && "All serialport callbacks should be removed before the global Controller instance is destroyed." );
#endif // _WITH_LIBUDEV
#endif // _DEBUG

		// Release the v7 javascript environment.
		v7_destroy( this->m_v7_js );

#ifdef _WITH_LIBUDEV
		this->m_udevMonitorThread.join();
#endif // _WITH_LIBUDEV

	};

	void Controller::start() {
#ifdef _DEBUG
		assert( g_webServer->isRunning() && "WebServer should be running before Controller is started." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		std::unique_lock<std::mutex> hardwareLock( this->m_hardwareMutex );

		// Fetch all the hardware from the database to initialize our local map of hardware instances.
		std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery(
			"SELECT `id`, `hardware_id`, `reference`, `type`, `enabled` "
			"FROM `hardware` "
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
			std::shared_ptr<Hardware> hardware = Hardware::factory( type, std::stoi( (*hardwareIt)["id"] ), (*hardwareIt)["reference"], parent );

			if ( (*hardwareIt)["enabled"] == "1" ) {
				hardware->start();
			}
			this->_installHardwareResourceHandlers( hardware );

			this->m_hardware.push_back( hardware );
		}
		hardwareLock.unlock();

		// Install the script- and timer resource handlers.
		this->_updateScriptResourceHandlers();
		this->_updateTimerResourceHandlers();

		// Add generic resource handlers for the hardware- and device resources.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller",
			"api/hardware", 100,
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( output_.is_null() ) {
							output_ = json::array();
						}
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "type" ) != input_.end()
								&& input_.find( "name" ) != input_.end()
							) {

								// Find the hardware type in the declared types from the hardware class.
								int type = -1;
								for ( auto typeIt = Hardware::TypeText.begin(); typeIt != Hardware::TypeText.end(); typeIt++ ) {
									if ( input_["type"].get<std::string>() == typeIt->second ) {
										type = typeIt->first;
									}
								}
								if ( type > -1 ) {
									std::string reference = randomString( 16 );
									// TODO remove the last settings parameter, not needed because hardware isn't started
									// automatically.
									auto hardware = this->declareHardware( static_cast<Hardware::Type>( type ), reference, { } );

									// A settings object can be passed along.
									/*
									is handled by hardware directly.
									if (
										input_.find( "settings") != input_.end()
										&& input_["settings"].is_object()
									) {
										std::map<std::string,std::string> settings;
										for ( auto settingsIt = input_["settings"].begin(); settingsIt != input_["settings"].end(); settingsIt++ ) {
											settings[settingsIt.key()] = settingsIt.value();
										}
										hardware->setSettings( settings );
									}
									*/

									output_["result"] = "OK";
									output_["hardware"] = hardware->getJson( true );
									g_webServer->touchResourceCallback( "controller" );
								} else {
									output_["result"] = "ERROR";
									output_["message"] = "Invalid type.";
									code_ = 400; // bad request
								}
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing fields.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to add hardware.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller",
			"api/devices", 100,
			WebServer::Method::GET,
			WebServer::t_callback( []( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				if ( output_.is_null() ) {
					output_ = json::array();
				}
			} )
		} ) ) );

#ifdef _WITH_LIBUDEV
		// libudev for detecting add- or remove usb device?
		// http://www.signal11.us/oss/udev/
		// If libudev is available we can use it to monitor disconenct and reconnect of the z-wave device. For
		// instance, the Aeon labs z-wave stick can be disconnected to bring it closer to the node when including.
		this->m_udev = udev_new();
		if ( this->m_udev ) {
			this->m_udevMonitor = udev_monitor_new_from_netlink( this->m_udev, "udev" );
			udev_monitor_filter_add_match_subsystem_devtype( this->m_udevMonitor, "tty", NULL );
			udev_monitor_enable_receiving( this->m_udevMonitor );

			// Start a new separate thread for the monitor.
			this->m_udevMonitorThread = std::thread( [this]() {
				while( ! this->m_shutdown ) {
					udev_device* dev = udev_monitor_receive_device( this->m_udevMonitor );
					if ( dev ) {
						std::string port = std::string( udev_device_get_devnode( dev ) );
						std::string action = std::string( udev_device_get_action( dev ) );
						std::lock_guard<std::mutex> lock( this->m_serialPortCallbacksMutex );
						for ( auto callbackIt = this->m_serialPortCallbacks.begin(); callbackIt != this->m_serialPortCallbacks.end(); callbackIt++ ) {
							callbackIt->second( port, action );
						}
						g_logger->logr( Logger::LogLevel::VERBOSE, this, "Detected %s %s.", port.c_str(), action.c_str() );
						udev_device_unref( dev );
					}
					std::this_thread::sleep_for( milliseconds( 250 ) );
				}
				udev_monitor_unref( this->m_udevMonitor );
				udev_unref( this->m_udev );
				
			} );
		} else {
			g_logger->log( Logger::LogLevel::WARNING, this, "Unable to  setup device monitoring." );
		}
#endif // _WITH_LIBUDEV

		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		g_webServer->removeResourceCallback( "controller" );
		g_webServer->removeResourceCallback( "controller-scripts" );
		g_webServer->removeResourceCallback( "controller-timers" );

		Worker::stop();

		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				auto hardware = (*hardwareIt);
				g_webServer->removeResourceCallback( "hardware-" + std::to_string( hardware->getId() ) );
				if ( hardware->isRunning() ) {
					hardware->stop();
				}
			}
			this->m_hardware.clear();
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Hardware> Controller::getHardware( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				return *hardwareIt;
			}
		}
		return nullptr;
	};

	std::vector<std::shared_ptr<Hardware> > Controller::getChildrenOfHardware( const Hardware& hardware_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		std::vector<std::shared_ptr<Hardware> > children;
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto parent = (*hardwareIt)->getParent();
			if (
				parent != nullptr
				&& parent.get() == &hardware_
			) {
				children.push_back( *hardwareIt );
			}
		}
		return children;

	};
	
	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::map<std::string, std::string> settings_, const bool& start_ ) {
		return this->declareHardware( type_, reference_, nullptr, settings_, start_ );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::map<std::string, std::string> settings_, const bool& start_ ) {
#ifdef _DEBUG
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
				"INSERT INTO `hardware` ( `hardware_id`, `reference`, `type`, `enabled` ) "
				"VALUES ( %d, %Q, %d, %d )",
				parent_->getId(), reference_.c_str(), static_cast<int>( type_ ),
				start_ ? 1 : 0
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `reference`, `type`, `enabled` ) "
				"VALUES ( %Q, %d, %d )",
				reference_.c_str(), static_cast<int>( type_ ),
				start_ ? 1 : 0
			);
		}

		// The lock is released while instantiating hardware because some hardware need to lookup their parent with
		// the getHardware* methods which also use the lock.
		lock.unlock();

		std::shared_ptr<Hardware> hardware = Hardware::factory( type_, id, reference_, parent_ );

		auto settings = hardware->getSettings();
		settings->insert( settings_ );
		if ( settings->isDirty() ) {
			settings->commit( *hardware );
		}

		if ( start_ ) {
			hardware->start();
		}
		this->_installHardwareResourceHandlers( hardware );

		// Reacquire lock when adding to the map to make it thread safe.
		lock = std::unique_lock<std::mutex>( this->m_hardwareMutex );
		this->m_hardware.push_back( hardware );
		lock.unlock();

		return hardware;
	};
	
	bool Controller::removeHardware( const std::shared_ptr<Hardware> hardware_ ) {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		bool result = false;

		// First all the childs are stopped and removed from the system. The database record is not yet
		// removed because that is done when the parent is removed by foreign key constraints.
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); ) {
			if ( (*hardwareIt)->getParent() == hardware_ ) {
				g_webServer->removeResourceCallback( "hardware-" + std::to_string( (*hardwareIt)->getId() ) );
				if ( (*hardwareIt)->isRunning() ) {
					(*hardwareIt)->stop();
				}

				hardwareIt = this->m_hardware.erase( hardwareIt );
				g_webServer->touchResourceCallback( "controller" );
			} else {
				hardwareIt++;
			}
		}
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); ) {
			if ( (*hardwareIt) == hardware_ ) {
				g_webServer->removeResourceCallback( "hardware-" + std::to_string( hardware_->getId() ) );
				if ( hardware_->isRunning() ) {
					hardware_->stop();
				}

				g_database->putQuery(
					"DELETE FROM `hardware` "
					"WHERE `id`=%d",
					hardware_->getId()
				);

				this->m_hardware.erase( hardwareIt );
				g_webServer->touchResourceCallback( "controller" );
				result = true;
				break;
			} else {
				hardwareIt++;
			}
		}
		
		return result;
	};

	std::shared_ptr<Device> Controller::getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->getDevice( reference_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->getDeviceById( id_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->getDeviceByName( name_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->getDeviceByLabel( label_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	template<class D> void Controller::newEvent( const D& device_, const unsigned int& source_ ) {
		// Events originating from scripts should not cause another script run.
		// TODO make this configurable per device?
		if (
			( source_ & Device::UpdateSource::SCRIPT ) != Device::UpdateSource::SCRIPT
			&& ( source_ & Device::UpdateSource::INIT ) != Device::UpdateSource::INIT
		) {
			auto interval = system_clock::now() - device_.getLastUpdate();

			json event;
			event["value"] = device_.getValue();
			event["previous_value"] = device_.getPreviousValue();
			event["interval"] = duration_cast<seconds>( interval ).count();
			event["source"] = source_;
			event["device"] = device_.getJson( false );

			// The processing of the event is deliberatly done in a separate method because this method is templated
			// and is essentially copied for each specialization.
			// TODO Only fetch those scripts that are present in the script / device crosstable.
			this->_runScripts( "event", event, g_database->getQuery(
				"SELECT s.`id`, s.`name`, s.`code`, s.`runs` "
				"FROM `scripts` s, `x_device_scripts` x "
				"WHERE x.`script_id`=s.`id` "
				"AND x.`device_id`=%d "
				"AND s.`enabled`=1 "
				"ORDER BY `id` ASC",
				device_.getId()
			) );
		}
	};
	template void Controller::newEvent( const Switch& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Level& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Counter& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Text& device_, const unsigned int& source_ );

#ifdef _WITH_LIBUDEV
	void Controller::addSerialPortCallback( const std::string& name_, const t_serialPortCallback& callback_ ) {
		std::lock_guard<std::mutex> lock( this->m_serialPortCallbacksMutex );
#ifdef _DEBUG
		assert( this->m_serialPortCallbacks.find( name_ ) == this->m_serialPortCallbacks.end() && "Serialport Callback name should be unique." );
#endif // _DEBUG
		this->m_serialPortCallbacks[name_] = callback_;
	};

	void Controller::removeSerialPortCallback( const std::string& name_ ) {
		std::lock_guard<std::mutex> lock( this->m_serialPortCallbacksMutex );
#ifdef _DEBUG
		assert( this->m_serialPortCallbacks.find( name_ ) != this->m_serialPortCallbacks.end() && "Serialport Callback name should exist before being removed." );
#endif // _DEBUG
		this->m_serialPortCallbacks.erase( name_ );
	};
#endif // _WITH_LIBUDEV

	milliseconds Controller::_work( const unsigned long int& iteration_ ) {
		auto now = system_clock::now();

		// See if we need to run the timers (every round minute).
		static unsigned int lastMinuteSinceEpoch = duration_cast<minutes>( now.time_since_epoch() ).count();
		unsigned int minuteSinceEpoch = duration_cast<minutes>( now.time_since_epoch() ).count();
		if ( lastMinuteSinceEpoch < minuteSinceEpoch ) {
			lastMinuteSinceEpoch = minuteSinceEpoch;
			this->_runTimers();
		}

		// Investigate the front of the queue for tasks that are due. If the next task is not due we're done due to
		// the fact that _scheduleTask keeps the list sorted on scheduled time.
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
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

		// This method should run a least every minute for timer jobs to run. The 5ms is a safe margin to make sure
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

	void Controller::_runScripts( const std::string& key_, const json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ ) {
		// Event processing is done in a separate thread to prevent scripts from blocking hardare updates.
		// TODO insert a task that checks if the script isn't running for more than xx seconds? This requires that
		// we don't detach and keep track of the thread.
		auto thread = std::thread( [this,key_,data_,scripts_]{
			std::lock_guard<std::mutex> lock( this->m_scriptsMutex );

			v7_val_t root = v7_get_global( this->m_v7_js );

			// Configure the v7 javascript environment with context data.
			v7_val_t dataObj;
			if ( V7_OK != v7_parse_json( this->m_v7_js, data_.dump().c_str(), &dataObj ) ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in scriptdata." );
#ifdef _DEBUG
				g_logger->log( Logger::LogLevel::VERBOSE, this, data_.dump().c_str() );
#endif // _DEBUG
				return;
			}
			v7_set( this->m_v7_js, root, key_.c_str(), ~0, dataObj );

			for ( auto scriptsIt = scripts_.begin(); scriptsIt != scripts_.end(); scriptsIt++ ) {

				v7_val_t js_result;
				v7_err js_error = v7_exec( this->m_v7_js, (*scriptsIt).at( "code" ).c_str(), &js_result );

				bool success = true;

				switch( js_error ) {
					case V7_SYNTAX_ERROR:
						g_logger->logr( Logger::LogLevel::ERROR, this, "Syntax error in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						success = false;
						break;
					case V7_EXEC_EXCEPTION:
						// Extract error message from result. Note that if the buffer is too small, v7 allocates it's
						// own memory chucnk which we need to free manually.
						char buffer[100], *p;
						p = v7_stringify( this->m_v7_js, js_result, buffer, sizeof( buffer ), V7_STRINGIFY_DEFAULT );
						g_logger->logr( Logger::LogLevel::ERROR, this, "Exception in in \"%s\" (%s).", (*scriptsIt).at( "name" ).c_str(), p );
						if ( p != buffer ) {
							free(p);
						}
						success = false;
						break;
					case V7_AST_TOO_LARGE:
					case V7_INTERNAL_ERROR:
						g_logger->logr( Logger::LogLevel::ERROR, this, "Internal error in in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						success = false;
						break;
					case V7_OK:
						g_logger->logr( Logger::LogLevel::NORMAL, this, "Script %s \"%s\" executed.", key_.c_str(), (*scriptsIt).at( "name" ).c_str() );
						break;
				}

				g_database->putQuery(
					"UPDATE `scripts` "
					"SET `runs`=`runs`+1, `enabled`=%d "
					"WHERE `id`=%q",
					success ? 1 : 0,
					(*scriptsIt).at( "id" ).c_str()
				);
				g_webServer->touchResourceCallback( "controller-scripts" );
			}

			// Remove the context data from the v7 environment.
			v7_del( this->m_v7_js, root, key_.c_str(), ~0 );

			// Get the userdata from the v7 environment and store it in settings for later use. The same logic
			// applies here as V7_EXEC_EXCEPTION regarding the buffer.
			// TODO do we need to do this after *every* script run, or maybe do this periodically.
			v7_val_t userDataObj = v7_get( this->m_v7_js, root, "userdata", ~0 );
			char buffer[1024], *p;
			p = v7_stringify( this->m_v7_js, userDataObj, buffer, sizeof( buffer ), V7_STRINGIFY_JSON );
			this->m_settings.put( "userdata", std::string( p ) );
			if ( p != buffer ) {
				free( p );
			}
			this->m_settings.commit();
		} );
		thread.detach();
	};

	void Controller::_runTimers() {
		std::lock_guard<std::mutex> lock( this->m_timersMutex );

		auto timers = g_database->getQuery(
			"SELECT DISTINCT c.`id`, c.`cron`, c.`name` "
			"FROM `timers` c, `x_timer_scripts` x, `scripts` s "
			"WHERE c.`id`=x.`timer_id` "
			"AND s.`id`=x.`script_id` "
			"AND c.`enabled`=1 "
			"AND s.`enabled`=1 "
			"ORDER BY c.`id` ASC"
		);
		for ( auto timerIt = timers.begin(); timerIt != timers.end(); timerIt++ ) {
			try {
				bool run = true;

				// Split the cron string into exactly 5 fields; m h dom mon dow.
				std::vector<std::pair<unsigned int, unsigned int> > extremes = { { 0, 59 }, { 0, 23 }, { 1, 31 }, { 1, 12 }, { 1, 7 } };
				auto fields = stringSplit( (*timerIt)["cron"], ' ' );
				if ( fields.size() != 5 ) {
					throw std::runtime_error( "invalid number of cron fields" );
				}
				for ( unsigned int field = 0; field <= 4; field++ ) {

					// Determine the values that are valid for each field by parsing the field and
					// filling an array with valid values.
					std::vector<std::string> subexpressions;
					if ( fields[field].find( "," ) != std::string::npos ) {
						subexpressions = stringSplit( fields[field], ',' );
					} else {
						subexpressions.push_back( fields[field] );
					}

					std::vector<unsigned int> rangeitems;

					for ( auto& subexpression : subexpressions ) {
						unsigned int start = extremes[field].first;
						unsigned int end = extremes[field].second;
						unsigned int modulo = 0;

						if ( subexpression.find( "/" ) != std::string::npos ) {
							auto parts = stringSplit( subexpression, '/' );
							if ( parts.size() != 2 ) {
								throw std::runtime_error( "invalid cron field devider" );
							}
							modulo = std::stoi( parts[1] );
							subexpression = parts[0];
						}

						if ( subexpression.find( "-" ) != std::string::npos ) {
							auto parts = stringSplit( subexpression, '-' );
							if ( parts.size() != 2 ) {
								throw std::runtime_error( "invalid cron field range" );
							}
							start = std::stoi( parts[0] );
							end = std::stoi( parts[1] );
						} else if (
							subexpression != "*"
							&& modulo == 0
						) {
							start = std::stoi( subexpression );
							end = start;
						}

						for ( unsigned int index = start; index <= end; index++ ) {
							if ( modulo > 0 ) {
								int remainder = index % modulo;
								if ( subexpression == "*" ) {
									if ( 0 == remainder ) {
										rangeitems.push_back( index );
									}
								} else if ( remainder == std::stoi( subexpression ) ) {
									rangeitems.push_back( index );
								}
							} else {
								rangeitems.push_back( index );
							}
						}
					}

					// Then we need to check if the current value for the field is available in the
					// array with valid items.
					time_t rawtime;
					time( &rawtime );

					struct tm* timeinfo;
					timeinfo = localtime( &rawtime );

					unsigned int current;
					switch( field ) {
						case 0: current = timeinfo->tm_min; break;
						case 1: current = timeinfo->tm_hour; break;
						case 2: current = timeinfo->tm_mday; break;
						case 3: current = timeinfo->tm_mon + 1; break;
						case 4: current = timeinfo->tm_wday == 0 ? 7 : timeinfo->tm_wday; break;
					}

					if ( std::find( rangeitems.begin(), rangeitems.end(), current ) == rangeitems.end() ) {
						run = false;
					}

					// If this field didn't match there's no need to investigate the other fields.
					if ( ! run ) {
						break;
					}
				}

				if ( run ) {
					json data = (*timerIt);
					this->_runScripts( "timer", data, g_database->getQuery(
						"SELECT s.`id`, s.`name`, s.`code`, s.`runs` "
						"FROM `x_timer_scripts` x, `scripts` s "
						"WHERE x.`script_id`=s.`id` "
						"AND x.`timer_id`=%q "
						"AND s.`enabled`=1 "
						"ORDER BY s.`id` ASC",
						(*timerIt)["id"].c_str()
					) );
				}

			} catch( std::exception ex_ ) {

				// Something went wrong while parsing the cron string. The timer is marked as disabled.
				g_logger->logr( Logger::LogLevel::ERROR, this, "Invalid cron for timer %s (%s).", (*timerIt).at( "name" ).c_str(), ex_.what() );
				g_database->putQuery(
					"UPDATE `timers` "
					"SET `enabled`=0 "
					"WHERE `id`=%q",
					(*timerIt).at( "id" ).c_str()
				);
			}
		}
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

	Controller::TaskOptions Controller::_parseTaskOptions( const std::string& options_ ) const {
		int lastTokenType = 0;
		TaskOptions result = { 0, 0, 1, 0, false, false };

		std::vector<std::string> optionParts;
		stringSplit( options_, ' ', optionParts );
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

	void Controller::_installHardwareResourceHandlers( const std::shared_ptr<Hardware> hardware_ ) {

		// The first handler to install is the list handler that outputs all the hardware available. Note that
		// the output json array is being populated by multiple resource handlers, each one adding another
		// piece of hardware.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( hardware_->getId() ),
			"api/hardware", 100,
			WebServer::Method::GET,
			WebServer::t_callback( [this,hardware_]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				if ( output_.is_null() ) {
					output_ = json::array();
				}

				// If a hardware_id property was provided, only devices belonging to that hardware id are being
				// added to the device list.
				auto hardwareIdIt = input_.find( "hardware_id" );
				if (
					hardwareIdIt != input_.end()
					&& (
						! hardware_->getParent()
						|| input_["hardware_id"].get<std::string>() != std::to_string( hardware_->getParent()->getId() )
					)
				) {
					return;
				} else if (
					hardwareIdIt == input_.end()
					&& hardware_->getParent()
				) {
					return;
				}

				// If the enabled flag was set only hardware that are enabled or disabled are returned.
				auto enabledIt = input_.find( "enabled" );
				if ( enabledIt != input_.end() ) {
					bool enabled = true;
					if ( input_["enabled"].is_boolean() ) {
						enabled = input_["enabled"].get<bool>();
					}
					if ( input_["enabled"].is_number() ) {
						enabled = input_["enabled"].get<unsigned int>() > 0;
					}
					if ( enabled && ! hardware_->isRunning() ) {
						return;
					}
					if ( ! enabled && hardware_->isRunning() ) {
						return;
					}
				}

				output_ += hardware_->getJson( false );
			} )
		} ) ) );

		// The second handler to install is the one that returns the details of a single piece of hardware and
		// the ability to update or delete the hardware.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( hardware_->getId() ),
			"api/hardware/" + std::to_string( hardware_->getId() ), 100,
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			WebServer::t_callback( [this,hardware_]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = hardware_->getJson( true );
						break;
					}
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {
						try {
							// A name property can be used to set the custom name for the hardware.
							if ( input_.find( "name" ) != input_.end() ) {
								hardware_->getSettings()->put( "name", input_["name"].get<std::string>() );
								hardware_->getSettings()->commit( *hardware_.get() );
							}

							// The enabled property can be used to enable or disable the hardware. For now this is only
							// possible on parent/main hardware, no children. Note that a restart is still possible if
							// the hardware specific resource handler needs it.
							bool enabled = hardware_->isRunning();
							if ( hardware_->getParent() == nullptr ) {
								if ( input_.find( "enabled") != input_.end() ) {
									if ( input_["enabled"].is_boolean() ) {
										enabled = input_["enabled"].get<bool>();
									}
									if ( input_["enabled"].is_number() ) {
										enabled = input_["enabled"].get<unsigned int>() > 0;
									}
									g_database->putQuery(
										"UPDATE `hardware` "
										"SET `enabled`=%d "
										"WHERE `id`=%d "
										"OR `hardware_id`=%d", // also children
										enabled ? 1 : 0,
										hardware_->getId(),
										hardware_->getId()
									);
								}
							}

							// Start or stop hardware. All children of the hardware are also started or stopped. This
							// is done in a separate thread to allow the client to return immediately.
							auto thread = std::thread( [this,enabled,hardware_]{
								std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
								if (
									! enabled
									|| hardware_->needsRestart()
								) {
									if ( hardware_->isRunning() ) {
										hardware_->stop();
									}
									for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware_
											&& (*hardwareIt)->isRunning()
										) {
											(*hardwareIt)->stop();
										}
									}
								}
								if ( enabled ) {
									if ( ! hardware_->isRunning() ) {
										hardware_->start();
									}
									for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware_
											&& ! (*hardwareIt)->isRunning()
										) {
											(*hardwareIt)->start();
										}
									}
								}
							} );
							thread.detach();

							output_["result"] = "OK";
							g_webServer->touchResourceCallback( "hardware-" + std::to_string( hardware_->getId() ) );
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to update hardware.";
							code_ = 400; // bad request
						}
						break;
					}
					case WebServer::Method::DELETE: {
						if ( this->removeHardware( hardware_ ) ) {
							output_["result"] = "OK";
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to update hardware.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
	};

	void Controller::_updateScriptResourceHandlers() const {

		// First all current callbacks for scripts are removed.
		g_webServer->removeResourceCallback( "controller-scripts" );

		// The first callback is the general script fetch callback that lists all available scripts and can
		// be used to add (post) new scripts.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-scripts",
			"api/scripts", 100,
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_scriptsMutex );
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = g_database->getQuery<json>(
							"SELECT `id`, `name`, `code`, `enabled`, `runs` "
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
								output_["result"] = "OK";
								output_["script"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `code`, `enabled`, `runs` "
									"FROM `scripts` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									scriptId
								);
								this->_updateScriptResourceHandlers();
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
				"api/scripts/" + std::to_string( scriptId ), 100,
				WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
				WebServer::t_callback( [this,scriptId]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
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
										"SET `name`=%Q, `code`=%Q, `runs`=0 "
										"WHERE `id`=%d",
										input_["name"].get<std::string>().c_str(),
										input_["code"].get<std::string>().c_str(),
										scriptId
									);
								}

								// The enabled property can be used to enable or disable the script.
								if ( input_.find( "enabled") != input_.end() ) {
									bool enabled = true;
									if ( input_["enabled"].is_boolean() ) {
										enabled = input_["enabled"].get<bool>();
									}
									if ( input_["enabled"].is_number() ) {
										enabled = input_["enabled"].get<unsigned int>() > 0;
									}
									g_database->putQuery(
										"UPDATE `scripts` "
										"SET `enabled`=%d "
										"WHERE `id`=%d ",
										enabled ? 1 : 0,
										scriptId
									);
								}

								output_["result"] = "OK";
								output_["script"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `code`, `enabled`, `runs` "
									"FROM `scripts` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									scriptId
								);
								g_webServer->touchResourceCallback( "controller-scripts" );

							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save script.";
								code_ = 400; // bad request
							}
							break;
						}
						case WebServer::Method::GET: {
							output_ = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `code`, `enabled`, `runs` "
								"FROM `scripts` "
								"WHERE `id`=%d "
								"ORDER BY `id` ASC",
								scriptId
							);
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
								output_["message"] = "Unable to delete script.";
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

	void Controller::_updateTimerResourceHandlers() const {

		// First all current callbacks for timers are removed.
		g_webServer->removeResourceCallback( "controller-timers" );

		const auto setScripts = []( const json& scriptIds_, unsigned int timerId_ ) {
			std::stringstream list;
			unsigned int index = 0;
			for ( auto scriptIdsIt = scriptIds_.begin(); scriptIdsIt != scriptIds_.end(); scriptIdsIt++ ) {
				auto scriptId = (*scriptIdsIt);
				if ( scriptId.is_number() ) {
					list << ( index > 0 ? "," : "" ) << scriptId;
					index++;
					g_database->putQuery(
						"INSERT OR IGNORE INTO `x_timer_scripts` "
						"(`timer_id`, `script_id`) "
						"VALUES (%d, %d)",
						timerId_,
						scriptId.get<unsigned int>()
					);
				}
			}
			g_database->putQuery(
				"DELETE FROM `x_timer_scripts` "
				"WHERE `timer_id`=%d "
				"AND `script_id` NOT IN (%q)",
				timerId_,
				list.str().c_str()
			);
		};

		// The first callback is the general timer fetch callback that lists all available timers and can
		// be used to add (post) new timers.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-timers",
			"api/timers", 100,
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this,&setScripts]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_timersMutex );
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = json::array();
						auto timers = g_database->getQuery<json>(
							"SELECT `id`, `name`, `cron`, `enabled` "
							"FROM `timers` "
							"ORDER BY `id` ASC"
						);
						for ( auto timersIt = timers.begin(); timersIt != timers.end(); timersIt++ ) {
							json timer = (*timersIt);
							timer["scripts"] = g_database->getQueryColumn<unsigned int>(
								"SELECT s.`id` "
								"FROM `scripts` s, `x_timer_scripts` x "
								"WHERE s.`id`=x.`script_id` "
								"AND x.`timer_id`=%d "
								"ORDER BY s.`id` ASC",
								(*timersIt)["id"].get<unsigned int>()
							);
							timer["devices"] = g_database->getQuery<json>(
								"SELECT d.`id`, x.`value` "
								"FROM `devices` d, `x_timer_devices` x "
								"WHERE d.`id`=x.`device_id` "
								"AND x.`timer_id`=%d "
								"ORDER BY d.`id` ASC",
								(*timersIt)["id"].get<unsigned int>()
							);
							output_ += timer;
						}
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "name") != input_.end()
								&& input_.find( "cron") != input_.end()
							) {
								auto timerId = g_database->putQuery(
									"INSERT INTO `timers` (`name`, `cron`) "
									"VALUES (%Q, %Q) ",
									input_["name"].get<std::string>().c_str(),
									input_["cron"].get<std::string>().c_str()
								);
								if (
									input_.find( "scripts") != input_.end()
									&& input_["scripts"].is_array()
								) {
									setScripts( input_["scripts"], timerId );
								}
								output_["result"] = "OK";
								output_["timer"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `cron` "
									"FROM `timers` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									timerId
								);
								this->_updateTimerResourceHandlers();
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing parameters.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to save timer.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );

		// Then a specific resource handler is installed for each timer. This handler allows for retrieval as
		// well as updating a timer.
		auto timerIds = g_database->getQueryColumn<unsigned int>(
			"SELECT `id` "
			"FROM `timers` "
			"ORDER BY `id` ASC"
		);
		for ( auto timerIdsIt = timerIds.begin(); timerIdsIt != timerIds.end(); timerIdsIt++ ) {
			unsigned int timerId = (*timerIdsIt);
			g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
				"controller-timers",
				"api/timers/" + std::to_string( timerId ), 100,
				WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
				WebServer::t_callback( [this,timerId,&setScripts]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
					std::lock_guard<std::mutex> lock( this->m_timersMutex );
					switch( method_ ) {
						case WebServer::Method::PUT: {
							try {
								if (
									input_.find( "name") != input_.end()
									&& input_.find( "cron") != input_.end()
								) {
									g_database->putQuery(
										"UPDATE `timers` "
										"SET `name`=%Q, `cron`=%Q "
										"WHERE `id`=%d",
										input_["name"].get<std::string>().c_str(),
										input_["cron"].get<std::string>().c_str(),
										timerId
									);
								}

								if (
									input_.find( "scripts") != input_.end()
									&& input_["scripts"].is_array()
								) {
									setScripts( input_["scripts"], timerId );
								}

								// The enabled property can be used to enable or disable the script.
								if ( input_.find( "enabled") != input_.end() ) {
									bool enabled = true;
									if ( input_["enabled"].is_boolean() ) {
										enabled = input_["enabled"].get<bool>();
									}
									if ( input_["enabled"].is_number() ) {
										enabled = input_["enabled"].get<unsigned int>() > 0;
									}
									g_database->putQuery(
										"UPDATE `timers` "
										"SET `enabled`=%d "
										"WHERE `id`=%d ",
										enabled ? 1 : 0,
										timerId
									);
								}

								output_["result"] = "OK";
								output_["timer"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `cron`, `enabled` "
									"FROM `timers` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									timerId
								);
								g_webServer->touchResourceCallback( "controller-timers" );

							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save timer.";
								code_ = 400; // bad request
							}
							break;
						}
						case WebServer::Method::GET: {
							output_ = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `cron`, `enabled` "
								"FROM `timers` "
								"WHERE `id`=%d "
								"ORDER BY `id` ASC",
								timerId
							);
							output_["scripts"] = g_database->getQueryColumn<unsigned int>(
								"SELECT s.`id` "
								"FROM `scripts` s, `x_timer_scripts` x "
								"WHERE s.`id`=x.`script_id` "
								"AND x.`timer_id`=%d "
								"ORDER BY s.`id` ASC",
								timerId
							);
							output_["devices"] = g_database->getQuery<json>(
								"SELECT d.`id`, x.`value` "
								"FROM `devices` d, `x_timer_devices` x "
								"WHERE d.`id`=x.`device_id` "
								"AND x.`timer_id`=%d "
								"ORDER BY d.`id` ASC",
								timerId
							);
							break;
						}
						case WebServer::Method::DELETE: {
							try {
								g_database->putQuery(
									"DELETE FROM `timers` "
									"WHERE `id`=%d",
									timerId
								);
								this->_updateTimerResourceHandlers();
								output_["result"] = "OK";
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to delete timer.";
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
