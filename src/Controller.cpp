#include <iostream>
#include <fstream>
#include <algorithm>
#include <future>

#include <sys/types.h>
#include <dirent.h>

#include "Controller.h"

#include "Logger.h"
#include "Database.h"
#include "Settings.h"
#include "WebServer.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

v7_err micasa_v7_update_device( struct v7* v7_, v7_val_t* res_ ) {
	micasa::Controller* controller = static_cast<micasa::Controller*>( v7_get_user_data( v7_, v7_get_global( v7_ ) ) );

	std::shared_ptr<micasa::Device> device = nullptr;
	v7_val_t arg0 = v7_arg( v7_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->getDeviceById( v7_get_int( v7_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->getDeviceByName( v7_get_string( v7_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->getDeviceByLabel( v7_get_string( v7_, &arg0, NULL ) );
		}
	}
	if ( device == nullptr ) {
		return v7_throwf( v7_, "Error", "Invalid device." );
	}

	std::string options = "";
	v7_val_t arg2 = v7_arg( v7_, 2 );
	if ( v7_is_string( arg2 ) ) {
		options = v7_get_string( v7_, &arg2, NULL );
	} else if ( ! v7_is_undefined( arg2 ) ) {
		return v7_throwf( v7_, "Error", "Invalid options." );
	}

	v7_val_t arg1 = v7_arg( v7_, 1 );
	if ( v7_is_number( arg1 ) ) {
		double value = v7_get_double( v7_, arg1 );
		if ( device->getType() == micasa::Device::Type::COUNTER ) {
			controller->_js_updateDevice( std::static_pointer_cast<micasa::Counter>( device ), value, options );
		} else if ( device->getType() == micasa::Device::Type::LEVEL ) {
			controller->_js_updateDevice( std::static_pointer_cast<micasa::Level>( device ), value, options );
		} else {
			return v7_throwf( v7_, "Error", "Invalid parameter for device." );
		}
	} else if ( v7_is_string( arg1 ) ) {
		std::string value = v7_get_string( v7_, &arg1, NULL );
		if ( device->getType() == micasa::Device::Type::SWITCH ) {
			controller->_js_updateDevice( std::static_pointer_cast<micasa::Switch>( device ), value, options );
		} else if ( device->getType() == micasa::Device::Type::TEXT ) {
			controller->_js_updateDevice( std::static_pointer_cast<micasa::Text>( device ), value, options );
		} else {
			return v7_throwf( v7_, "Error", "Invalid parameter for device." );
		}
	} else {
		return v7_throwf( v7_, "Error", "Invalid parameter for device." );
	}

	return V7_OK;
};

v7_err micasa_v7_get_device( struct v7* v7_, v7_val_t* res_ ) {
	micasa::Controller* controller = static_cast<micasa::Controller*>( v7_get_user_data( v7_, v7_get_global( v7_ ) ) );

	std::shared_ptr<micasa::Device> device = nullptr;
	v7_val_t arg0 = v7_arg( v7_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->getDeviceById( v7_get_int( v7_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->getDeviceByName( v7_get_string( v7_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->getDeviceByLabel( v7_get_string( v7_, &arg0, NULL ) );
		}
	}
	if ( device == nullptr ) {
		return v7_throwf( v7_, "Error", "Invalid device." );
	}

	v7_err js_error = v7_parse_json( v7_, device->getJson().dump().c_str(), res_ );
	if ( V7_OK != js_error ) {
		return v7_throwf( v7_, "Error", "Internal error." );
	}

	return V7_OK;
};

v7_err micasa_v7_get_data( struct v7* v7_, v7_val_t* res_ ) {
	micasa::Controller* controller = static_cast<micasa::Controller*>( v7_get_user_data( v7_, v7_get_global( v7_ ) ) );

	std::shared_ptr<micasa::Device> device = nullptr;
	v7_val_t arg0 = v7_arg( v7_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->getDeviceById( v7_get_int( v7_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->getDeviceByName( v7_get_string( v7_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->getDeviceByLabel( v7_get_string( v7_, &arg0, NULL ) );
		}
	}
	if ( device == nullptr ) {
		return v7_throwf( v7_, "Error", "Invalid device." );
	}

	v7_val_t arg1 = v7_arg( v7_, 1 );
	int range;
	if ( v7_is_number( arg1 ) ) {
		range = v7_get_int( v7_, arg1 );
	} else {
		return v7_throwf( v7_, "Error", "Invalid range." );
	}

	v7_val_t arg2 = v7_arg( v7_, 2 );
	std::string interval;
	if ( v7_is_string( arg2 ) ) {
		interval = v7_get_string( v7_, &arg2, NULL );
	} else {
		return v7_throwf( v7_, "Error", "Invalid interval." );
	}

	v7_val_t arg3 = v7_arg( v7_, 3 );
	std::string group = "day";
	if ( v7_is_string( arg3 ) ) {
		group = v7_get_string( v7_, &arg3, NULL );
	}

	v7_err js_error;
	switch( device->getType() ) {
		case micasa::Device::Type::COUNTER: {
			js_error = v7_parse_json( v7_, std::static_pointer_cast<micasa::Counter>( device )->getData( range, interval, group ).dump().c_str(), res_ );
			break;
		}
		case micasa::Device::Type::LEVEL: {
			js_error = v7_parse_json( v7_, std::static_pointer_cast<micasa::Level>( device )->getData( range, interval, group ).dump().c_str(), res_ );
			break;
		}
		case micasa::Device::Type::SWITCH: {
			js_error = v7_parse_json( v7_, std::static_pointer_cast<micasa::Switch>( device )->getData( range, interval ).dump().c_str(), res_ );
			break;
		}
		case micasa::Device::Type::TEXT: {
			js_error = v7_parse_json( v7_, std::static_pointer_cast<micasa::Text>( device )->getData( range, interval ).dump().c_str(), res_ );
			break;
		}
	}

	if ( V7_OK != js_error ) {
		return v7_throwf( v7_, "Error", "Internal error." );
	}

	return V7_OK;
};

v7_err micasa_v7_include( struct v7* v7_, v7_val_t* res_ ) {
	micasa::Controller* controller = static_cast<micasa::Controller*>( v7_get_user_data( v7_, v7_get_global( v7_ ) ) );

	v7_val_t arg0 = v7_arg( v7_, 0 );
	std::string script;
	if (
		v7_is_string( arg0 )
		&& controller->_js_include( v7_get_string( v7_, &arg0, NULL ), script )
	) {
		v7_val_t js_result;
		return v7_exec( v7_, script.c_str(), &js_result );
	} else {
		return v7_throwf( v7_, "Error", "Invalid script name." );
	}
}

v7_err micasa_v7_log( struct v7* v7_, v7_val_t* res_ ) {
	micasa::Controller* controller = static_cast<micasa::Controller*>( v7_get_user_data( v7_, v7_get_global( v7_ ) ) );

	v7_val_t arg0 = v7_arg( v7_, 0 );
	char buffer[100], *p;
	if ( v7_is_object( arg0 ) ) {
		p = v7_stringify( v7_, arg0, buffer, sizeof( buffer ), V7_STRINGIFY_JSON );
	} else {
		p = v7_stringify( v7_, arg0, buffer, sizeof( buffer ), V7_STRINGIFY_DEFAULT );
	}
	micasa::Logger::log( micasa::Logger::LogLevel::NOTICE, controller, std::string( p ) );
	if ( p != buffer ) {
		free(p);
	}

	return V7_OK;
}

namespace micasa {

	using namespace std::chrono;
	using namespace nlohmann;

	extern std::unique_ptr<Database> g_database;
	extern std::unique_ptr<Settings<>> g_settings;
	extern std::unique_ptr<WebServer> g_webServer;

	Controller::Controller() :
		m_running( false )
	{
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before global Controller instance." );
#endif // _DEBUG

		std::lock_guard<std::mutex> lock( this->m_jsMutex );

		this->m_v7_js = v7_create();
		v7_val_t root = v7_get_global( this->m_v7_js );
		v7_set_user_data( this->m_v7_js, root, this );

		v7_val_t userDataObj;
		if ( V7_OK != v7_parse_json( this->m_v7_js, g_settings->get( CONTROLLER_SETTING_USERDATA, "{}" ).c_str(), &userDataObj ) ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Syntax error in userdata." );
			if ( V7_OK != v7_parse_json( this->m_v7_js, "{}", &userDataObj ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Syntax error in default userdata." );
			}
		}
		v7_def( this->m_v7_js, root, "userdata", ~0, V7_PROPERTY_NON_CONFIGURABLE, userDataObj );

		v7_set_method( this->m_v7_js, root, "updateDevice", &micasa_v7_update_device );
		v7_set_method( this->m_v7_js, root, "getDevice", &micasa_v7_get_device );
		v7_set_method( this->m_v7_js, root, "getData", &micasa_v7_get_data );
		v7_set_method( this->m_v7_js, root, "include", &micasa_v7_include );
		v7_set_method( this->m_v7_js, root, "log", &micasa_v7_log );

		v7_def( this->m_v7_js, root, "SOURCE_PLUGIN", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_TIMER", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::TIMER ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_SCRIPT", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::SCRIPT ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_API", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::API ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_LINK", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::LINK ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_SYSTEM", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::SYSTEM ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_USER", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::USER ) ) );
		v7_def( this->m_v7_js, root, "SOURCE_EVENT", ~0, V7_PROPERTY_NON_CONFIGURABLE, v7_mk_number( this->m_v7_js, Device::resolveUpdateSource( Device::UpdateSource::EVENT ) ) );
	};

	Controller::~Controller() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after global Controller instance." );
#ifdef _WITH_LIBUDEV
		assert( this->m_serialPortCallbacks.size() == 0 && "All serialport callbacks should be removed before the global Controller instance is destroyed." );
#endif // _WITH_LIBUDEV
#endif // _DEBUG

		// Release the v7 javascript environment.
		v7_destroy( this->m_v7_js );
	};

	void Controller::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		// Fetch all the plugins from the database to initialize our local map of plugin instances. NOTE parents
		// always have a higher id than clients, so the query order should make sure parents are created first and are
		// present when childs are created.
		std::unique_lock<std::recursive_mutex> pluginsLock( this->m_pluginsMutex );
		std::vector<std::map<std::string, std::string>> pluginsData = g_database->getQuery(
			"SELECT `id`, `plugin_id`, `reference`, `type`, `enabled` "
			"FROM `plugins` "
			"ORDER BY `id` ASC"
		);
		for ( auto& pluginData : pluginsData ) {
			std::shared_ptr<Plugin> parent;
			if ( pluginData["plugin_id"].size() > 0 ) {
				parent = this->getPluginById( std::stoi( pluginData["plugin_id"] ) );
			}

			// The plugin is then created and immeidately initialized. NOTE the init method is necessary because the
			// plugin cannot initialize it's devices if it's not already wrapped in a shared pointer.
			std::shared_ptr<Plugin> plugin = Plugin::factory(
				Plugin::resolveTextType( pluginData["type"] ),
				std::stoi( pluginData["id"] ),
				pluginData["reference"],
				parent
			);
			plugin->init();
			this->m_plugins[pluginData["reference"]] = plugin;

			// Only parent plugin is started automatically. The plugin itself should take care of starting it's
			// children (for instance, right after declarePlugin). Starting the plugin is done in a separate thread
			// to work around the pluginsMutex being locked.
			if (
				pluginData["enabled"] == "1"
				&& parent == nullptr
			) {
				this->m_scheduler.schedule( 0, 1, this, [plugin]( std::shared_ptr<Scheduler::Task<>> ) {
					plugin->start();
				} );
			}
		}
		pluginsLock.unlock();

		// Start a task that runs at every whole minute that processes the configured timers. The 5ms is a safe margin
		// to make sure the whole minute has passed.
		auto now = system_clock::now();
		auto wait = now + ( milliseconds( 60005 ) - duration_cast<milliseconds>( now.time_since_epoch() ) % milliseconds( 60000 ) );
		this->m_scheduler.schedule( wait, 60000, SCHEDULER_REPEAT_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			if ( this->m_running ) {
				this->_runTimers();
			}
		} );

		this->m_running = true;

#ifdef _WITH_LIBUDEV
		// If libudev is available we can use it to monitor disconenct and reconnect of the z-wave device. For instance
		// the Aeon labs z-wave stick can be disconnected to bring it closer to the node when including. NOTE that udev
		// is using it's own thread because the scheduler would cause way too much overhead.
		this->m_udev = udev_new();
		if ( this->m_udev ) {
			this->m_udevMonitor = udev_monitor_new_from_netlink( this->m_udev, "udev" );
			udev_monitor_filter_add_match_subsystem_devtype( this->m_udevMonitor, "tty", NULL );
			udev_monitor_enable_receiving( this->m_udevMonitor );

			this->m_udevWorker = std::thread( [this]() {
				do {
					std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
					udev_device* dev = udev_monitor_receive_device( this->m_udevMonitor );
					if ( dev ) {
						std::string port = std::string( udev_device_get_devnode( dev ) );
						std::string action = std::string( udev_device_get_action( dev ) );
						std::lock_guard<std::mutex> lock( this->m_serialPortCallbacksMutex );
						for ( auto callbackIt = this->m_serialPortCallbacks.begin(); callbackIt != this->m_serialPortCallbacks.end(); callbackIt++ ) {
							callbackIt->second( port, action );
						}
						Logger::logr( Logger::LogLevel::VERBOSE, this, "Detected %s %s.", port.c_str(), action.c_str() );
						udev_device_unref( dev );
					}
				} while( this->m_running );
			} );
		} else {
			Logger::log( Logger::LogLevel::WARNING, this, "Unable to setup device monitoring." );
		}
#endif // _WITH_LIBUDEV

		Logger::log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase();
		this->m_running = false;

#ifdef _WITH_LIBUDEV
		if ( this->m_udev ) {
			this->m_udevWorker.join();
			udev_monitor_unref( this->m_udevMonitor );
			udev_unref( this->m_udev );
		}
#endif // _WITH_LIBUDEV

		// Stopping the plugins is done asynchroniously. First all parent plugin instances are ordered to stop in a
		// separate thread...
		std::unique_lock<std::recursive_mutex> pluginsLock( this->m_pluginsMutex );
		std::map<std::string, std::future<void>> futures;
		for ( auto& pluginPair : this->m_plugins ) {
			auto plugin = pluginPair.second;
			if (
				plugin->getParent() == nullptr
				&& plugin->getState() != Plugin::State::DISABLED
			) {
				futures[plugin->getName()] = std::async( std::launch::async, [plugin] {
					plugin->stop();
				} );
			}
		}
		pluginsLock.unlock();

		// ... then all threads are waited for to complete, skipping over plugin threads that take too long to stop.
		for ( auto const &futuresIt : futures ) {
			std::future_status status = futuresIt.second.wait_for( seconds( 15 ) );
			if ( status == std::future_status::timeout ) {
				Logger::logr( Logger::LogLevel::ERROR, this, "Unable to stop %s within allowed timeframe.", futuresIt.first.c_str() );
			}
#ifdef _DEBUG
			assert( status == std::future_status::ready && "Plugin should stop properly." );
#endif // _DEBUG
		}

		pluginsLock.lock();
		this->m_plugins.clear();
		pluginsLock.unlock();

		Logger::log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Plugin> Controller::getPlugin( const std::string& reference_ ) const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		try {
			return this->m_plugins.at( reference_ );
		} catch( std::out_of_range ex_ ) {
			return nullptr;
		}
	};

	std::shared_ptr<Plugin> Controller::getPluginById( const unsigned int& id_ ) const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		for ( auto const &pluginIt : this->m_plugins ) {
			if ( pluginIt.second->getId() == id_ ) {
				return pluginIt.second;
			}
		}
		return nullptr;
	};

	std::vector<std::shared_ptr<Plugin>> Controller::getAllPlugins() const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		std::vector<std::shared_ptr<Plugin>> all;
		for ( auto const &pluginIt : this->m_plugins ) {
			all.push_back( pluginIt.second );
		}
		return all;
	};

	std::shared_ptr<Plugin> Controller::declarePlugin( const Plugin::Type type_, const std::string reference_, const std::vector<Setting>& settings_, bool enabled_ ) {
		return this->declarePlugin( type_, reference_, nullptr, settings_, enabled_ );
	};

	std::shared_ptr<Plugin> Controller::declarePlugin( const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_, const std::vector<Setting>& settings_, bool enabled_ ) {
		std::unique_lock<std::recursive_mutex> lock( this->m_pluginsMutex );
		try {
			return this->m_plugins.at( reference_ );
		} catch( std::out_of_range ex_ ) { /* does not exists */ }

		long id;
		if ( parent_ ) {
			id = g_database->putQuery(
				"INSERT INTO `plugins` ( `plugin_id`, `reference`, `type`, `enabled` ) "
				"VALUES ( %d, %Q, %Q, %d )",
				parent_->getId(),
				reference_.c_str(),
				Plugin::resolveTextType( type_ ).c_str(),
				enabled_ ? 1 : 0
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `plugins` ( `reference`, `type`, `enabled` ) "
				"VALUES ( %Q, %Q, %d )",
				reference_.c_str(),
				Plugin::resolveTextType( type_ ).c_str(),
				enabled_ ? 1 : 0
			);
		}

		std::shared_ptr<Plugin> plugin = Plugin::factory( type_, id, reference_, parent_ );
		this->m_plugins[reference_] = plugin;

		auto settings = plugin->getSettings();
		settings->insert( settings_ );
		if ( settings->isDirty() ) {
			settings->commit();
		}

		json data = json::object();
		data["event"] = "plugin_add";
		data["data"] = plugin->getJson();
		g_webServer->broadcast( data.dump() );

		return plugin;
	};

	void Controller::removePlugin( const std::shared_ptr<Plugin> plugin_ ) {

		// NOTE all the children records in the database will be removed automatically due to
		// foreign key constraints.
		g_database->putQuery(
			"DELETE FROM `plugins` "
			"WHERE `id`=%d",
			plugin_->getId()
		);

		// Then the plugin is stopped in a separate thread.
		this->m_scheduler.schedule( 0, 1, this, [this,plugin_]( std::shared_ptr<Scheduler::Task<>> ) {
			auto future = std::async( std::launch::async, [plugin_] {
				plugin_->stop();
			} );
			std::future_status status = future.wait_for( seconds( 15 ) );
			if ( status == std::future_status::timeout ) {
				Logger::logr( Logger::LogLevel::ERROR, this, "Unable to stop %s within allowed timeframe.", plugin_->getName().c_str() );
			}
#ifdef _DEBUG
			assert( status == std::future_status::ready && "Plugin should stop properly." );
#endif // _DEBUG
		} );

		// All children and the plugin itself are removed from the list.
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		for ( auto pluginsIt = this->m_plugins.begin(); pluginsIt != this->m_plugins.end(); ) {
			auto plugin = pluginsIt->second;
			if (
				plugin == plugin_
				|| plugin->getParent() == plugin_
			) {
				json data = json::object();
				data["event"] = "plugin_remove";
				data["data"] = {
					{ "id", plugin->getId() }
				};
				g_webServer->broadcast( data.dump() );

				pluginsIt = this->m_plugins.erase( pluginsIt );
			} else {
				pluginsIt++;
			}
		}
	};

	std::shared_ptr<Device> Controller::getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		for ( auto const &pluginsIt : this->m_plugins ) {
			auto device = pluginsIt.second->getDevice( reference_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		for ( auto const &pluginsIt : this->m_plugins ) {
			auto device = pluginsIt.second->getDeviceById( id_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		for ( auto const &pluginsIt : this->m_plugins ) {
			auto device = pluginsIt.second->getDeviceByName( name_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		for ( auto const &pluginsIt : this->m_plugins ) {
			auto device = pluginsIt.second->getDeviceByLabel( label_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::vector<std::shared_ptr<Device>> Controller::getAllDevices() const {
		std::lock_guard<std::recursive_mutex> lock( this->m_pluginsMutex );
		std::vector<std::shared_ptr<Device>> result;
		for ( auto const &pluginsIt : this->m_plugins ) {
			auto devices = pluginsIt.second->getAllDevices();
			result.insert( result.end(), devices.begin(), devices.end() );
		}
		return result;
	};

	bool Controller::isScheduled( std::shared_ptr<const Device> device_ ) const {
		return this->m_scheduler.first(
			[device_]( const Scheduler::BaseTask& task_ ) -> bool {
				return task_.data == device_.get();
			}
		) != nullptr;
	};

	template<class D> void Controller::newEvent( std::shared_ptr<D> device_, const Device::UpdateSource& source_ ) {
		if ( this->m_running ) {

			if ( ( source_ & Device::UpdateSource::LINK ) != Device::UpdateSource::LINK ) {
				this->_runLinks( device_ );
			}

			if ( ( source_ & Device::UpdateSource::SCRIPT ) != Device::UpdateSource::SCRIPT ) {

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
					json event;
					event["value"] = device_->getValue();
					event["device"] = device_->getJson();
					this->_runScripts( "event", event, scripts );
				}
			}

			json device = device_->getJson();
			json data = json::object();
			data["event"] = "device_update";
			data["data"] = {
				{ "id", device_->getId() },
				{ "plugin_id", device_->getPlugin()->getId() },
				{ "value", device["value"] },
				{ "source", Device::resolveUpdateSource( source_ ) }
			};
			g_webServer->broadcast( data.dump() );
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

	template<class D> void Controller::_processTask( const std::shared_ptr<D> device_, const typename D::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ ) {
		if ( options_.clear ) {
			this->m_scheduler.erase(
				[device_]( const Scheduler::BaseTask& task_ ) -> bool {
					return task_.data == device_.get();
				}
			);
		}

		// If the recur option was set, no script or timer source is send along with the update. This way updating the
		// device will cause additional scripts to be executed.
		Device::UpdateSource source = source_;
		if ( options_.recur ) {
			source = Device::resolveUpdateSource( 0 );
		}

		// A mutable copy is made from the passed const options. This is necessary because even though we need to use
		// the mutable flag on the lambda, the original type copied into the lambda is const and mutable won't change
		// that. So now, a non const is copied into the lamda as const and the mutable flag will allow it to be changed.
		TaskOptions options = options_;
		options.repeat--;
		options.clear = false;

		// Tasks are scheduled synchroniously, one after another, to make sure the tasks are executed sequentially. The
		// device is captured as a weak pointer to prevent the scheduler from blocking the destruction of devices. This
		// also prevents a shared_ptr cycle (!).
		std::weak_ptr<D> devicePtr = device_;
		this->m_scheduler.schedule( 1000 * options.afterSec, 1, device_.get(), [this,devicePtr,source,value_,options]( std::shared_ptr<Scheduler::Task<>> ) mutable {
			auto device = devicePtr.lock();
			if ( device ) {
				typename D::t_value current = device->getValue();
				device->updateValue( source, value_ );

				options.afterSec = options.repeatSec;

				if ( options.forSec > 0.000001 ) {
					this->m_scheduler.schedule( 1000 * options.forSec, 1, device.get(), [this,devicePtr,source,value_,options,current]( std::shared_ptr<Scheduler::Task<>> ) mutable {
						auto device = devicePtr.lock();
						if ( device ) {
							device->updateValue( source, current );
							if ( options.repeat > 0 ) {
								this->_processTask( device, value_, source, options );
							}
						}
					} );
				} else if ( options.repeat > 0 ) {
					this->_processTask( device, value_, source, options );
				}
			}
		} );
	};
	template void Controller::_processTask( const std::shared_ptr<Level> device_, const typename Level::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
	template void Controller::_processTask( const std::shared_ptr<Counter> device_, const typename Counter::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
	template void Controller::_processTask( const std::shared_ptr<Text> device_, const typename Text::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
	template void Controller::_processTask( const std::shared_ptr<Switch> device_, const typename Switch::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );

	void Controller::_runScripts( const std::string& key_, const json& data_, const std::vector<std::map<std::string, std::string>>& scripts_ ) {
		this->m_scheduler.schedule( 0, 1, this, [this,key_,data_,scripts_]( std::shared_ptr<Scheduler::Task<>> ) {
			std::lock_guard<std::mutex> jsLock( this->m_jsMutex );

			// Configure the v7 javascript environment with context data.
			v7_val_t root = v7_get_global( this->m_v7_js );
			v7_val_t dataObj;
			if ( V7_OK != v7_parse_json( this->m_v7_js, data_.dump().c_str(), &dataObj ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Syntax error in scriptdata." );
#ifdef _DEBUG
				Logger::log( Logger::LogLevel::VERBOSE, this, data_.dump().c_str() );
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
						Logger::logr( Logger::LogLevel::ERROR, this, "Syntax error in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						success = false;
						break;
					case V7_EXEC_EXCEPTION:
						// Extract error message from result. NOTE 1 that if the buffer is too small, v7 allocates it's
						// own memory chunk which we need to free manually. NOTE 2 these exceptions will not result in
						// the script getting disabled, so the success flag is still true.
						char buffer[100], *p;
						p = v7_stringify( this->m_v7_js, js_result, buffer, sizeof( buffer ), V7_STRINGIFY_DEFAULT );
						Logger::logr( Logger::LogLevel::ERROR, this, "Exception in in \"%s\" (%s).", (*scriptsIt).at( "name" ).c_str(), p );
						if ( p != buffer ) {
							free(p);
						}
						break;
					case V7_AST_TOO_LARGE:
					case V7_INTERNAL_ERROR:
						Logger::logr( Logger::LogLevel::ERROR, this, "Internal error in in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						success = false;
						break;
					case V7_OK:
						Logger::logr( Logger::LogLevel::NORMAL, this, "Script %s \"%s\" executed.", key_.c_str(), (*scriptsIt).at( "name" ).c_str() );
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
			g_settings->put( CONTROLLER_SETTING_USERDATA, std::string( p ) );
			if ( p != buffer ) {
				free( p );
			}
			g_settings->commit();
		} );
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
				std::vector<std::pair<unsigned int, unsigned int>> extremes = { { 0, 59 }, { 0, 23 }, { 1, 31 }, { 1, 12 }, { 1, 7 } };
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
							if ( device ) {
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
				}

			} catch( std::exception ex_ ) {

				// Something went wrong while parsing the cron string. The timer is marked as disabled.
				Logger::logr( Logger::LogLevel::ERROR, this, "Invalid cron for timer %s (%s).", (*timerIt).at( "name" ).c_str(), ex_.what() );
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
				if ( targetDevice ) {
					TaskOptions options = { 0, 0, 1, 0, false, false };
					if ( (*linksIt)["after"].size() > 0 ) {
						options.afterSec = std::stod( (*linksIt)["after"] );
					}
					if ( (*linksIt)["for"].size() > 0 ) {
						options.forSec = std::stod( (*linksIt)["for"] );
					}
					if ( (*linksIt)["clear"].size() > 0 ) {
						options.clear = std::stoi( (*linksIt)["clear"] ) > 0;
					}
					this->_processTask<Switch>( targetDevice, (*linksIt)["target_value"], Device::UpdateSource::LINK, options );
				}
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

	template<class D> void Controller::_js_updateDevice( const std::shared_ptr<D> device_, const typename D::t_value& value_, const std::string& options_ ) {
		TaskOptions options = this->_parseTaskOptions( options_ );
		this->_processTask( device_, value_, Device::UpdateSource::SCRIPT, options );
	};
	template void Controller::_js_updateDevice( const std::shared_ptr<Level> device_, const typename Level::t_value& value_, const std::string& options_ );
	template void Controller::_js_updateDevice( const std::shared_ptr<Counter> device_, const typename Counter::t_value& value_, const std::string& options_ );
	template void Controller::_js_updateDevice( const std::shared_ptr<Switch> device_, const typename Switch::t_value& value_, const std::string& options_ );
	template void Controller::_js_updateDevice( const std::shared_ptr<Text> device_, const typename Text::t_value& value_, const std::string& options_ );

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

}; // namespace micasa
