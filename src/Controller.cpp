// v7
// https://docs.cesanta.com/v7/master/

#include <iostream>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <dirent.h>

#include "Controller.h"

#include "Logger.h"
#include "Database.h"
#include "Settings.h"

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
			*res_ = v7_mk_boolean( js_, controller->_js_updateDevice( std::static_pointer_cast<micasa::Counter>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::LEVEL ) {
			*res_ = v7_mk_boolean( js_, controller->_js_updateDevice( std::static_pointer_cast<micasa::Level>( device ), value, options ) );
		} else {
			return v7_throwf( js_, "Error", "Invalid parameter for device." );
		}
	} else if ( v7_is_string( arg1 ) ) {
		std::string value = v7_get_string( js_, &arg1, NULL );
		if ( device->getType() == micasa::Device::Type::SWITCH ) {
			*res_ = v7_mk_boolean( js_, controller->_js_updateDevice( std::static_pointer_cast<micasa::Switch>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::TEXT ) {
			*res_ = v7_mk_boolean( js_, controller->_js_updateDevice( std::static_pointer_cast<micasa::Text>( device ), value, options ) );
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
		case micasa::Device::Type::COUNTER:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Counter>( device )->getJson( false ).dump().c_str(), res_ );
			break;
		case micasa::Device::Type::LEVEL:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Level>( device )->getJson( false ).dump().c_str(), res_ );
			break;
		case micasa::Device::Type::SWITCH:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Switch>( device )->getJson( false ).dump().c_str(), res_ );
			break;
		case micasa::Device::Type::TEXT:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Text>( device )->getJson( false ).dump().c_str(), res_ );
			break;
	}

	if ( V7_OK != js_error ) {
		return v7_throwf( js_, "Error", "Internal error." );
	}

	return V7_OK;
};

v7_err micasa_v7_include( struct v7* js_, v7_val_t* res_ ) {
	micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );
	
	v7_val_t arg0 = v7_arg( js_, 0 );
	std::string script;
	if (
		v7_is_string( arg0 )
		&& controller->_js_include( v7_get_string( js_, &arg0, NULL ), script )
	) {
		v7_val_t js_result;
		return v7_exec( js_, script.c_str(), &js_result );
	} else {
		return v7_throwf( js_, "Error", "Invalid script name." );
	}
}

v7_err micasa_v7_log( struct v7* js_, v7_val_t* res_ ) {
	micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );
	
	v7_val_t arg0 = v7_arg( js_, 0 );
	std::string log;
	if ( v7_is_string( arg0 ) ) {
		controller->_js_log( v7_get_string( js_, &arg0, NULL ) );
	} else if ( v7_is_number( arg0 ) ) {
		controller->_js_log( std::to_string( v7_get_double( js_, arg0 ) ) );
	} else {
		return v7_throwf( js_, "Error", "Invalid log value." );
	}
	return V7_OK;
}

namespace micasa {

	using namespace std::chrono;
	using namespace nlohmann;

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Settings<> > g_settings;

	template<> void Controller::Task::setValue<Level>( const typename Level::t_value& value_ ) { this->levelValue = value_; };
	template<> void Controller::Task::setValue<Counter>( const typename Counter::t_value& value_ ) { this->counterValue = value_; };
	template<> void Controller::Task::setValue<Switch>( const typename Switch::t_value& value_ ) { this->switchValue = value_; };
	template<> void Controller::Task::setValue<Text>( const typename Text::t_value& value_ ) { this->textValue = value_; };

	Controller::Controller(): Worker() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before global Controller instance." );
		assert( g_logger && "Global Logger instance should be created before global Controller instance." );
#endif // _DEBUG

		// Initialize the v7 javascript environment.
		this->m_v7_js = v7_create();
		v7_val_t root = v7_get_global( this->m_v7_js );
		v7_set_user_data( this->m_v7_js, root, this );

		v7_val_t userDataObj;
		if ( V7_OK != v7_parse_json( this->m_v7_js, g_settings->get( "userdata", "{}" ).c_str(), &userDataObj ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in userdata." );
			if ( V7_OK != v7_parse_json( this->m_v7_js, "{}", &userDataObj ) ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in default userdata." );
			}
		}
		v7_set( this->m_v7_js, root, "userdata", ~0, userDataObj );

		v7_set_method( this->m_v7_js, root, "updateDevice", &micasa_v7_update_device );
		v7_set_method( this->m_v7_js, root, "getDevice", &micasa_v7_get_device );
		v7_set_method( this->m_v7_js, root, "include", &micasa_v7_include );
		v7_set_method( this->m_v7_js, root, "log", &micasa_v7_log );
	};

	Controller::~Controller() {
#ifdef _DEBUG
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
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		std::unique_lock<std::mutex> hardwareLock( this->m_hardwareMutex );

		// Fetch all the hardware from the database to initialize our local map of hardware instances.
		std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery(
			"SELECT `id`, `hardware_id`, `reference`, `type`, `enabled` "
			"FROM `hardware` "
			"ORDER BY `id` ASC"
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

			Hardware::Type type = Hardware::resolveType( (*hardwareIt)["type"] );
			std::shared_ptr<Hardware> hardware = Hardware::factory( type, std::stoi( (*hardwareIt)["id"] ), (*hardwareIt)["reference"], parent );

			// Only parent hardware is started automatically. The hardware itself should take care of
			// starting their children (for instance, right after declareHardware). Starting the hardware
			// is done in a separate thread to work around the hardwareMutex being locked and hardware
			// might want to declare hardware in their startup code.
			if (
				(*hardwareIt)["enabled"] == "1"
				&& parent == nullptr
			) {
				std::thread( [hardware]() {
					hardware->start();
				} ).detach();
			}

			this->m_hardware.push_back( hardware );
		}
		hardwareLock.unlock();

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
			g_logger->log( Logger::LogLevel::WARNING, this, "Unable to setup device monitoring." );
		}
#endif // _WITH_LIBUDEV

		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		Worker::stop();

		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				auto hardware = (*hardwareIt);
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

	std::shared_ptr<Hardware> Controller::getHardwareById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getId() == id_ ) {
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
	
	std::vector<std::shared_ptr<Hardware> > Controller::getAllHardware() const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		return std::vector<std::shared_ptr<Hardware> >( this->m_hardware );
	};
	
	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::vector<Setting>& settings_, const bool& start_ ) {
		return this->declareHardware( type_, reference_, nullptr, settings_, start_ );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::vector<Setting>& settings_, const bool& start_ ) {
		std::unique_lock<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				if ( start_ ) {
					(*hardwareIt)->start();
				}
				return *hardwareIt;
			}
		}

		long id;
		if ( parent_ ) {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `hardware_id`, `reference`, `type`, `enabled` ) "
				"VALUES ( %d, %Q, %Q, %d )",
				parent_->getId(),
				reference_.c_str(),
				Hardware::resolveType( type_ ).c_str(),
				start_ ? 1 : 0
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `reference`, `type`, `enabled` ) "
				"VALUES ( %Q, %Q, %d )",
				reference_.c_str(),
				Hardware::resolveType( type_ ).c_str(),
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
			settings->commit();
		}

		if ( start_ ) {
			hardware->start();
		}

		// Reacquire lock when adding to the map to make it thread safe.
		lock = std::unique_lock<std::mutex>( this->m_hardwareMutex );
		this->m_hardware.push_back( hardware );
		lock.unlock();

		return hardware;
	};
	
	void Controller::removeHardware( const std::shared_ptr<Hardware> hardware_ ) {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );

		// First all the childs are stopped and removed from the system. The database record is not yet
		// removed because that is done when the parent is removed by foreign key constraints.
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); ) {
			if ( (*hardwareIt)->getParent() == hardware_ ) {
				if ( (*hardwareIt)->isRunning() ) {
					(*hardwareIt)->stop();
				}
				hardwareIt = this->m_hardware.erase( hardwareIt );
			} else {
				hardwareIt++;
			}
		}
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); ) {
			if ( (*hardwareIt) == hardware_ ) {
				if ( hardware_->isRunning() ) {
					hardware_->stop();
				}

				g_database->putQuery(
					"DELETE FROM `hardware` "
					"WHERE `id`=%d",
					hardware_->getId()
				);

				this->m_hardware.erase( hardwareIt );
				break;
			} else {
				hardwareIt++;
			}
		}
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

	std::vector<std::shared_ptr<Device> > Controller::getAllDevices() const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		std::vector<std::shared_ptr<Device> > result;
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto devices = (*hardwareIt)->getAllDevices();
			result.insert( result.end(), devices.begin(), devices.end() );
		}
		return result;
	};

	bool Controller::isScheduled( std::shared_ptr<const Device> device_ ) const {
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
		for ( auto taskIt = this->m_taskQueue.begin(); taskIt != this->m_taskQueue.end(); taskIt++ ) {
			if ( (*taskIt)->device == device_ ) {
				return true;
			}
		}
		return false;
	};

	template<class D> void Controller::newEvent( std::shared_ptr<D> device_, const Device::UpdateSource& source_ ) {
		if ( ( source_ & Device::UpdateSource::INIT ) != Device::UpdateSource::INIT ) {

			if ( ( source_ & Device::UpdateSource::LINK ) != Device::UpdateSource::LINK ) {
				this->_runLinks( device_ );
			}

			if ( ( source_ & Device::UpdateSource::SCRIPT ) != Device::UpdateSource::SCRIPT ) {
				json event;
				event["value"] = device_->getValue();
				event["previous_value"] = device_->getPreviousValue();
				event["source"] = Device::resolveUpdateSource( source_ );
				event["device"] = device_->getJson( false );

				// NOTE The processing of the event is deliberatly done in a separate method because this method is
				// templated and is essentially copied for each specialization.
				auto scripts = g_database->getQuery(
					"SELECT s.`id`, s.`name`, s.`code` "
					"FROM `scripts` s, `x_device_scripts` x "
					"WHERE x.`script_id`=s.`id` "
					"AND x.`device_id`=%d "
					"AND s.`enabled`=1 "
					"ORDER BY `id` ASC",
					device_->getId()
				);
				if ( scripts.size() > 0 ) {
					this->_runScripts( "event", event, scripts );
				}
			}
		}
	};

	template void Controller::newEvent( std::shared_ptr<Switch> device_, const Device::UpdateSource& source_ );
	template void Controller::newEvent( std::shared_ptr<Level> device_, const Device::UpdateSource& source_ );
	template void Controller::newEvent( std::shared_ptr<Counter> device_, const Device::UpdateSource& source_ );
	template void Controller::newEvent( std::shared_ptr<Text> device_, const Device::UpdateSource& source_ );

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
		// the fact that _scheduleTask keeps the list sorted on scheduled time. All tasks that are due are gathered in
		// a separate vector so the task queue mutex can be released before updating devices.
		std::vector<std::shared_ptr<Task> > tasks;
		{
			std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
			auto taskQueueIt = this->m_taskQueue.begin();
			while(
				taskQueueIt != this->m_taskQueue.end()
				&& (*taskQueueIt)->scheduled <= now + milliseconds( 5 )
			) {
				tasks.push_back( *taskQueueIt );
				taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
			}
		}
		for ( auto tasksIt = tasks.begin(); tasksIt != tasks.end(); tasksIt++ ) {
			std::shared_ptr<Task> task = (*tasksIt);
			switch( task->device->getType() ) {
				case Device::Type::COUNTER:
					std::static_pointer_cast<Counter>( task->device )->updateValue( task->source, task->counterValue );
					break;
				case Device::Type::LEVEL:
					std::static_pointer_cast<Level>( task->device )->updateValue( task->source, task->levelValue );
					break;
				case Device::Type::SWITCH: {
					std::static_pointer_cast<Switch>( task->device )->updateValue( task->source, task->switchValue );
					break;
				}
				case Device::Type::TEXT:
					std::static_pointer_cast<Text>( task->device )->updateValue( task->source, task->textValue );
					break;
			}
		}

		// This method should run a least every minute for timer jobs to run. The 5ms is a safe margin to make sure
		// the whole minute has passed.
		auto wait = milliseconds( 60005 ) - duration_cast<milliseconds>( now.time_since_epoch() ) % milliseconds( 60000 );

		// If there's nothing in the queue anymore we can wait the default duration before investigating the queue
		// again. This is because the _scheduleTask method will wake us up if there's work to do anyway.
		{
			std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
			auto taskQueueIt = this->m_taskQueue.begin();
			if ( taskQueueIt != this->m_taskQueue.end() ) {
				auto delay = duration_cast<milliseconds>( (*taskQueueIt)->scheduled - now );
				if ( delay < wait ) {
					wait = delay;
				}
			}
		}

		return wait;
	};

	template<class D> bool Controller::_processTask( const std::shared_ptr<D> device_, const typename D::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ ) {
		if ( options_.clear ) {
			this->_clearTaskQueue( device_ );
		}

		typename D::t_value previousValue = device_->getValue();
		for ( int i = 0; i < abs( options_.repeat ); i++ ) {
			double delaySec = options_.afterSec + ( i * options_.forSec ) + ( i * options_.repeatSec );

			// NOTE if the recur option was set we're not providing SCRIPT as the source of the update. This way
			// the event handler will execute scripts even when the update comes from a script.
			Task task = { device_, options_.recur ? Device::resolveUpdateSource( 0 ) : source_, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
			task.setValue<D>( value_ );
			this->_scheduleTask( std::make_shared<Task>( task ) );

			if (
				options_.forSec > 0.05
				&& (
					options_.repeat > 0
					|| i < abs( options_.repeat ) - 1
				)
			) {
				delaySec += options_.forSec;
				Task task = { device_, options_.recur ? Device::resolveUpdateSource( 0 ) : source_, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
				task.setValue<D>( previousValue );
				this->_scheduleTask( std::make_shared<Task>( task ) );
			}
		}

		return true;
	};
	template bool Controller::_processTask( const std::shared_ptr<Level> device_, const typename Level::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
	template bool Controller::_processTask( const std::shared_ptr<Counter> device_, const typename Counter::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
	template bool Controller::_processTask( const std::shared_ptr<Text> device_, const typename Text::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
	
	// The switch variant of the method is specialized separately because it uses the opposite value instead of the
	// previous value for the "for"-option.
	template<> bool Controller::_processTask( const std::shared_ptr<Switch> device_, const typename Switch::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ ) {
		if ( options_.clear ) {
			this->_clearTaskQueue( device_ );
		}

		for ( int i = 0; i < abs( options_.repeat ); i++ ) {
			double delaySec = options_.afterSec + ( i * options_.forSec ) + ( i * options_.repeatSec );

			// NOTE if the recur option was set we're not providing SCRIPT as the source of the update. This way
			// the event handler will execute scripts even when the update comes from a script.
			Task task = { device_, options_.recur ? Device::resolveUpdateSource( 0 ) : source_, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
			task.setValue<Switch>( value_ );
			this->_scheduleTask( std::make_shared<Task>( task ) );

			if (
				options_.forSec > 0.05
				&& (
					options_.repeat > 0
					|| i < abs( options_.repeat ) - 1
				)
			) {
				delaySec += options_.forSec;
				Task task = { device_, options_.recur ? Device::resolveUpdateSource( 0 ) : source_, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
				task.setValue<Switch>( Switch::getOppositeValue( value_ ) );
				this->_scheduleTask( std::make_shared<Task>( task ) );
			}
		}

		return true;
	};

	void Controller::_runScripts( const std::string& key_, const json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ ) {
		auto thread = std::thread( [this,key_,data_,scripts_]{
			std::lock_guard<std::mutex> lock( this->m_jsMutex );

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
				
				if ( ! success ) {
					g_database->putQuery(
						"UPDATE `scripts` "
						"SET `enabled`=0 "
						"WHERE `id`=%q",
						(*scriptsIt).at( "id" ).c_str()
					);
				}
			}

			// Remove the context data from the v7 environment.
			v7_del( this->m_v7_js, root, key_.c_str(), ~0 );

			// Get the userdata from the v7 environment and store it in settings for later use. The same logic
			// applies here as V7_EXEC_EXCEPTION regarding the buffer.
			v7_val_t userDataObj = v7_get( this->m_v7_js, root, "userdata", ~0 );
			char buffer[1024], *p;
			p = v7_stringify( this->m_v7_js, userDataObj, buffer, sizeof( buffer ), V7_STRINGIFY_JSON );
			g_settings->put( "userdata", std::string( p ) );
			if ( p != buffer ) {
				free( p );
			}
			g_settings->commit();
		} );
		thread.detach();
	};

	void Controller::_runTimers() {
		auto timers = g_database->getQuery(
			"SELECT DISTINCT `id`, `cron`, `name` "
			"FROM `timers` "
			"WHERE `enabled`=1 "
			"ORDER BY `id` ASC"
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
					
					// First run the scripts that are associated with this timer.
					json data = (*timerIt);
					auto scripts = g_database->getQuery(
						"SELECT s.`id`, s.`name`, s.`code` "
						"FROM `x_timer_scripts` x, `scripts` s "
						"WHERE x.`script_id`=s.`id` "
						"AND x.`timer_id`=%q "
						"AND s.`enabled`=1 "
						"ORDER BY s.`id` ASC",
						(*timerIt)["id"].c_str()
					);
					if ( scripts.size() > 0 ) {
						this->_runScripts( "timer", data, scripts );
					}
					
					// Then update the devices that are associated with this timer.
					auto devices = g_database->getQuery(
						"SELECT x.`device_id`, x.`value` "
						"FROM `x_timer_devices` x, `devices` d "
						"WHERE x.`device_id`=d.`id` "
						"AND x.`timer_id`=%q "
						"AND d.`enabled`=1 "
						"ORDER BY d.`id` ASC",
						(*timerIt)["id"].c_str()
					);
					if ( devices.size() > 0 ) {
						for ( auto devicesIt = devices.begin(); devicesIt != devices.end(); devicesIt++ ) {
							auto device = this->getDeviceById( std::stoi( (*devicesIt)["device_id"] ) );
							TaskOptions options = { 0, 0, 1, 0, false, false };
							switch( device->getType() ) {
								case Device::Type::COUNTER:
									this->_processTask<Counter>( std::static_pointer_cast<Counter>( device ), std::stoi( (*devicesIt)["value"] ), Device::UpdateSource::TIMER, options );
									break;
								case Device::Type::LEVEL:
									this->_processTask<Level>( std::static_pointer_cast<Level>( device ), std::stod( (*devicesIt)["value"] ), Device::UpdateSource::TIMER, options );
									break;
								case Device::Type::SWITCH:
									this->_processTask<Switch>( std::static_pointer_cast<Switch>( device ), (*devicesIt)["value"], Device::UpdateSource::TIMER, options );
									break;
								case Device::Type::TEXT:
									this->_processTask<Text>( std::static_pointer_cast<Text>( device ), (*devicesIt)["value"], Device::UpdateSource::TIMER, options );
									break;
							}
						}
					}
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

	void Controller::_runLinks( std::shared_ptr<Device> device_ ) {

		// Action links are only supported for switch-type devices. Other devices CAN be linked but cannot perform
		// any target actions.
		if ( device_->getType() == Device::Type::SWITCH ) {
			auto device = std::static_pointer_cast<Switch>( device_ );
			auto links = g_database->getQuery(
				"SELECT `target_device_id`, `target_value`, `after`, `for`, `clear` "
				"FROM `links` "
				"WHERE `device_id`=%d "
				"AND `target_device_id`!=%d "
				"AND `target_device_id` IS NOT NULL "
				"AND `value`=%Q "
				"AND `target_value` IS NOT NULL "
				"AND `enabled`=1 "
				"ORDER BY `id`",
				device->getId(),
				device->getId(),
				device->getValue().c_str()
			);

			for ( auto linksIt = links.begin(); linksIt != links.end(); linksIt++ ) {
				auto targetDevice = std::static_pointer_cast<Switch>( this->getDeviceById( std::stoi( (*linksIt)["target_device_id"] ) ) );
				TaskOptions options = { 0, 0, 1, 0, false, false };
				if ( (*linksIt)["after"].size() > 0 ) {
					options.afterSec = std::stoi( (*linksIt)["after"] );
				}
				if ( (*linksIt)["for"].size() > 0 ) {
					options.forSec = std::stoi( (*linksIt)["for"] );
				}
				if ( (*linksIt)["clear"].size() > 0 ) {
					options.clear = std::stoi( (*linksIt)["clear"] ) > 0;
				}
				this->_processTask<Switch>( targetDevice, (*linksIt)["target_value"], Device::UpdateSource::LINK, options );
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

	void Controller::_clearTaskQueue( const std::shared_ptr<Device> device_ ) {
		std::unique_lock<std::mutex> lock( this->m_taskQueueMutex );
		for ( auto taskQueueIt = this->m_taskQueue.begin(); taskQueueIt != this->m_taskQueue.end(); ) {
			if ( (*taskQueueIt)->device == device_ ) {
				taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
			} else {
				taskQueueIt++;
			}
		}
	};

	Controller::TaskOptions Controller::_parseTaskOptions( const std::string& options_ ) const {
		int lastTokenType = 0;
		TaskOptions result = { 0, 0, 1, 0, false, false };

		std::vector<std::string> optionParts = stringSplit( options_, ' ' );
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

	template<class D> bool Controller::_js_updateDevice( const std::shared_ptr<D> device_, const typename D::t_value& value_, const std::string& options_ ) {
		TaskOptions options = this->_parseTaskOptions( options_ );
		return this->_processTask( device_, value_, Device::UpdateSource::SCRIPT, options );
	};
	template bool Controller::_js_updateDevice( const std::shared_ptr<Level> device_, const typename Level::t_value& value_, const std::string& options_ );
	template bool Controller::_js_updateDevice( const std::shared_ptr<Counter> device_, const typename Counter::t_value& value_, const std::string& options_ );
	template bool Controller::_js_updateDevice( const std::shared_ptr<Switch> device_, const typename Switch::t_value& value_, const std::string& options_ );
	template bool Controller::_js_updateDevice( const std::shared_ptr<Text> device_, const typename Text::t_value& value_, const std::string& options_ );

	bool Controller::_js_include( const std::string& name_, std::string& script_ ) {
		// This is done without a lock because it is called from a script that is executed while holding the lock.
		try {
			script_ = g_database->getQueryValue<std::string>(
				"SELECT `code` "
				"FROM `scripts` "
				"WHERE `name`=%Q "
				"AND `enabled`=1",
				name_.c_str()
			);
			return true;
		} catch( const Database::NoResultsException& ex_ ) {
			return false;
		}
	};

	void Controller::_js_log( const std::string& log_ ) {
		g_logger->log( Logger::LogLevel::SCRIPT, log_ );
	};

}; // namespace micasa
