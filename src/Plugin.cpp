#include <iostream>
#include <sstream>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Plugin.h"

#include "Logger.h"
#include "Database.h"
#include "Controller.h"
#include "WebServer.h"

#include "device/Level.h"
#include "device/Counter.h"
#include "device/Text.h"
#include "device/Switch.h"

#ifdef _DEBUG
	#include "plugins/Debug.h"
#endif // _DEBUG
#include "plugins/Dummy.h"
#include "plugins/HarmonyHub.h"
#ifdef _WITH_HOMEKIT
	#include "plugins/HomeKit.h"
#endif // _WITH_HOMEKIT
#ifdef _WITH_LINUX_SPI
	#include "plugins/PiFace.h"
	#include "plugins/PiFaceBoard.h"
#endif // _WITH_LINUX_SPI
#include "plugins/RFXCom.h"
#include "plugins/SolarEdge.h"
#include "plugins/SolarEdgeInverter.h"
#include "plugins/Telegram.h"
#include "plugins/WeatherUnderground.h"
#ifdef _WITH_OPENZWAVE
	#include "plugins/ZWave.h"
	#include "plugins/ZWaveNode.h"
#endif // _WITH_OPENZWAVE

namespace micasa {

	using namespace std::chrono;
	using namespace nlohmann;

	extern std::unique_ptr<Database> g_database;
	extern std::unique_ptr<Controller> g_controller;
	extern std::unique_ptr<WebServer> g_webServer;

	const char* Plugin::settingsName = "plugin";

	const std::map<Plugin::Type, std::string> Plugin::TypeText = {
#ifdef _DEBUG
		{ Plugin::Type::DEBUG, "debug" },
#endif // _DEBUG
		{ Plugin::Type::DUMMY, "dummy" },
		{ Plugin::Type::HARMONY_HUB, "harmony_hub" },
#ifdef _WITH_HOMEKIT
		{ Plugin::Type::HOMEKIT, "homekit" },
#endif // _WITH_LINUX_SPI
#ifdef _WITH_LINUX_SPI
		{ Plugin::Type::PIFACE, "piface" },
		{ Plugin::Type::PIFACE_BOARD, "piface_board" },
#endif // _WITH_LINUX_SPI
		{ Plugin::Type::RFXCOM, "rfxcom" },
		{ Plugin::Type::SOLAREDGE, "solaredge" },
		{ Plugin::Type::SOLAREDGE_INVERTER, "solaredge_inverter" },
		{ Plugin::Type::TELEGRAM, "telegram" },
		{ Plugin::Type::WEATHER_UNDERGROUND, "weather_underground" },
#ifdef _WITH_OPENZWAVE
		{ Plugin::Type::ZWAVE, "zwave" },
		{ Plugin::Type::ZWAVE_NODE, "zwave_node" },
#endif // _WITH_OPENZWAVE
	};

	const std::map<Plugin::State, std::string> Plugin::StateText = {
		{ Plugin::State::INIT, "Initializing" },
		{ Plugin::State::READY, "Ready" },
		{ Plugin::State::DISABLED, "Disabled" },
		{ Plugin::State::FAILED, "Failed" },
		{ Plugin::State::SLEEPING, "Sleeping" },
		{ Plugin::State::DISCONNECTED, "Disconnected" }
	};

	Plugin::Plugin( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : m_id( id_ ), m_type( type_ ), m_reference( reference_ ), m_parent( parent_ ) {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before Plugins instances." );
#endif // _DEBUG
		this->m_settings = std::make_shared<Settings<Plugin>>( *this );
	};

	Plugin::~Plugin() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after Plugin instances." );
		assert( this->m_state == Plugin::State::DISABLED && "Plugin should be stopped before being destructed." );
#endif // _DEBUG
		std::lock_guard<std::mutex> devicesLock( this->m_devicesMutex );
		this->m_devices.clear();
	};

	std::shared_ptr<Plugin> Plugin::factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) {
		switch( type_ ) {
#ifdef _DEBUG
			case Type::DEBUG:
				return std::make_shared<Debug>( id_, type_, reference_, parent_ );
				break;
#endif // _DEBUG
			case Type::DUMMY:
				return std::make_shared<Dummy>( id_, type_, reference_, parent_ );
				break;
			case Type::HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, type_, reference_, parent_ );
				break;
#ifdef _WITH_HOMEKIT
			case Type::HOMEKIT:
				return std::make_shared<HomeKit>( id_, type_, reference_, parent_ );
				break;
#endif // _WITH_HOMEKIT
#ifdef _WITH_LINUX_SPI
			case Type::PIFACE:
				return std::make_shared<PiFace>( id_, type_, reference_, parent_ );
				break;
			case Type::PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, type_, reference_, parent_ );
				break;
#endif // _WITH_LINUX_SPI
			case Type::RFXCOM:
				return std::make_shared<RFXCom>( id_, type_, reference_, parent_ );
				break;
			case Type::SOLAREDGE:
				return std::make_shared<SolarEdge>( id_, type_, reference_, parent_ );
				break;
			case Type::SOLAREDGE_INVERTER:
				return std::make_shared<SolarEdgeInverter>( id_, type_, reference_, parent_ );
				break;
			case Type::TELEGRAM:
				return std::make_shared<Telegram>( id_, type_, reference_, parent_ );
				break;
			case Type::WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, type_, reference_, parent_ );
				break;
#ifdef _WITH_OPENZWAVE
			case Type::ZWAVE:
				return std::make_shared<ZWave>( id_, type_, reference_, parent_ );
				break;
			case Type::ZWAVE_NODE:
				return std::make_shared<ZWaveNode>( id_, type_, reference_, parent_ );
				break;
#endif // _WITH_OPENZWAVE
		}
		return nullptr;
	}

	void Plugin::init() {
		std::lock_guard<std::mutex> devicesLock( this->m_devicesMutex );
		std::vector<std::map<std::string, std::string>> devicesData = g_database->getQuery(
			"SELECT `id`, `reference`, `label`, `type`, `enabled` "
			"FROM `devices` "
			"WHERE `plugin_id`=%d",
			this->m_id
		);
		for ( auto const &devicesDataIt : devicesData ) {
			std::shared_ptr<Device> device = Device::factory(
				this->shared_from_this(),
				Device::resolveTextType( devicesDataIt.at( "type" ) ),
				std::stoi( devicesDataIt.at( "id" ) ),
				devicesDataIt.at( "reference" ),
				devicesDataIt.at( "label" ),
				( devicesDataIt.at( "enabled" ) == "1" )
			);
			this->m_devices[devicesDataIt.at( "reference" )] = device;
		}
	};

	void Plugin::start() {
		std::unique_lock<std::mutex> devicesLock( this->m_devicesMutex );
		for ( auto const &deviceIt : this->m_devices ) {
			if ( deviceIt.second->isEnabled() ) {
				deviceIt.second->start();
			}
		}
		devicesLock.unlock();

		// Set the state to initializing if the plugin itself didn't already set the state to something else.
		if ( this->getState() == State::DISABLED ) {
			this->setState( Plugin::State::INIT );
		}

		Logger::log( this->m_parent == nullptr ? Logger::LogLevel::NORMAL : Logger::LogLevel::VERBOSE, this, "Started." );
	};

	void Plugin::stop() {
		std::unique_lock<std::mutex> devicesLock( this->m_devicesMutex );
		for ( auto const &deviceIt : this->m_devices ) {
			if ( deviceIt.second->isEnabled() ) {
				deviceIt.second->stop();
			}
		}
		devicesLock.unlock();

		if ( this->m_settings->isDirty() ) {
			this->m_settings->commit();
		}
		if ( this->getState() != State::DISABLED ) {
			this->setState( State::DISABLED );
		}
		Logger::log( this->m_parent == nullptr ? Logger::LogLevel::NORMAL : Logger::LogLevel::VERBOSE, this, "Stopped." );
	};

	std::string Plugin::getName() const {
		return this->m_settings->get( "name", this->getLabel() );
	};

	void Plugin::setState( const State& state_, bool children_ ) {
		this->m_state = state_;
		if ( children_ ) {
			for ( auto& child : this->getChildren() ) {
				child->setState( state_ );
			}
		}

		json data = json::object();
		data["event"] = "plugin_update";
		data["data"] = this->getJson();
		g_webServer->broadcast( data.dump() );
	};

	json Plugin::getJson() const {
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "enabled", this->getState() != Plugin::State::DISABLED },
			{ "type", Plugin::resolveTextType( this->m_type ) },
			{ "state", Plugin::resolveTextState( this->m_state ) }
		};
		return result;
	};

	json Plugin::getSettingsJson() const {
		json result = json::array();

		json setting = {
			{ "name", "name" },
			{ "label", "Name" },
			{ "type", "string" },
			{ "maxlength", 64 },
			{ "minlength", 3 },
			{ "mandatory", true },
			{ "sort", 1 }
		};
		result += setting;

		// Child plugin is enabled and disabled by it's parent.
		if ( this->getParent() == nullptr ) {
			setting = {
				{ "name", "enabled" },
				{ "label", "Enabled" },
				{ "type", "boolean" },
				{ "default", false },
				{ "sort", 2 }
			};
			result += setting;
		}

		return result;
	};

	std::vector<std::shared_ptr<Plugin>> Plugin::getChildren() const {
		auto me = this->shared_from_this();
		std::vector<std::shared_ptr<Plugin>> children;
		for ( auto& plugin : g_controller->getAllPlugins() ) {
			if ( plugin->getParent() == me ) {
				children.push_back( plugin );
			}
		}
		return children;
	};

	std::shared_ptr<Device> Plugin::getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		try {
			return this->m_devices.at( reference_ );
		} catch( std::out_of_range ex_ ) {
			return nullptr;
		}
	};

	std::shared_ptr<Device> Plugin::getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getId() == id_ ) {
				return devicesIt.second;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Plugin::getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getName() == name_ ) {
				return devicesIt.second;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Plugin::getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getLabel() == label_ ) {
				return devicesIt.second;
			}
		}
		return nullptr;
	};

	std::vector<std::shared_ptr<Device>> Plugin::getAllDevices() const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		std::vector<std::shared_ptr<Device>> all;
		for ( auto const &devicesIt : this->m_devices ) {
			all.push_back( devicesIt.second );
		}
		return all;
	};

	std::vector<std::shared_ptr<Device>> Plugin::getAllDevices( const std::string& prefix_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		std::vector<std::shared_ptr<Device>> result;
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getReference().substr( 0, prefix_.size() ) == prefix_ ) {
				result.push_back( devicesIt.second );
			}
		}
		return result;
	}

	void Plugin::removeDevice( const std::shared_ptr<Device> device_ ) {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( devicesIt->second == device_ ) {
				if ( device_->isEnabled() ) {
					device_->stop();
				}
				g_database->putQuery(
					"DELETE FROM `devices` "
					"WHERE `id`=%d",
					device_->getId()
				);

				json data = json::object();
				data["event"] = "device_remove";
				data["data"] = {
					{ "id", device_->getId() }
				};
				g_webServer->broadcast( data.dump() );

				this->m_devices.erase( devicesIt );
				break;
			}
		}
	};

	template<class T> std::shared_ptr<T> Plugin::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		try {
			std::shared_ptr<T> device = std::static_pointer_cast<T>( this->m_devices.at( reference_ ) );

			// System settings (settings that start with an underscore and are usually defined by #define)
			// should always overwrite existing system settings.
			for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
				if ( (*settingsIt).first.at( 0 ) == '_' ) {
					device->getSettings()->put( settingsIt->first, settingsIt->second );
				}
			}
			if ( device->getSettings()->isDirty() ) {
				device->getSettings()->commit();
			}

			return device;
		} catch( std::out_of_range ex_ ) { /* does not exists */ }

		long id = g_database->putQuery(
			"INSERT INTO `devices` ( `plugin_id`, `reference`, `type`, `label`, `enabled` ) "
			"VALUES ( %d, %Q, %Q, %Q, 0 )",
			this->m_id,
			reference_.c_str(),
			Device::resolveTextType( T::type ).c_str(),
			label_.c_str()
		);
		std::shared_ptr<T> device = std::static_pointer_cast<T>( Device::factory( this->shared_from_this(), T::type, id, reference_, label_, false ) );

		auto settings = device->getSettings();
		settings->insert( settings_ );
		if ( settings->isDirty() ) {
			settings->commit();
		}

		this->m_devices[reference_] = device;

		json data = json::object();
		data["event"] = "device_add";
		data["data"] = device->getJson();
		g_webServer->broadcast( data.dump() );

		return device;
	};
	template std::shared_ptr<Counter> Plugin::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
	template std::shared_ptr<Level> Plugin::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
	template std::shared_ptr<Switch> Plugin::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
	template std::shared_ptr<Text> Plugin::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );

	bool Plugin::_queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto pendingUpdate = this->m_pendingUpdates[reference_];
		if ( __likely(
			pendingUpdate == nullptr
			|| pendingUpdate->waitFor( blockNewUpdate_ )
		) ) {
			this->m_pendingUpdates[reference_] = this->m_scheduler.schedule<t_pendingUpdate>( waitForResult_, 1, this, [this,reference_,source_,data_]( std::shared_ptr<Scheduler::Task<t_pendingUpdate>> ) -> t_pendingUpdate {
				std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
				this->m_pendingUpdates.erase( reference_ );
				return { source_, data_ };
			} );
			return true;
		} else {
			return false;
		}
	};

	bool Plugin::_queuePendingUpdate( const std::string& reference_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_queuePendingUpdate( reference_, dummy, data_, blockNewUpdate_, waitForResult_ );
	};

	bool Plugin::_queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		std::string dummy = "";
		return this->_queuePendingUpdate( reference_, source_, dummy, blockNewUpdate_, waitForResult_ );
	};

	bool Plugin::_queuePendingUpdate( const std::string& reference_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_queuePendingUpdate( reference_, dummy, blockNewUpdate_, waitForResult_ );
	};

	bool Plugin::_releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_, std::string& data_ ) {
		std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		try {
			auto pendingUpdate = this->m_pendingUpdates.at( reference_ );
			pendingUpdatesLock.unlock(); // the task itself requires a lock on the pending updates aswell
			this->m_scheduler.proceed( 0, pendingUpdate );
			t_pendingUpdate update = pendingUpdate->wait();
			source_ |= update.source;
			data_ = update.data;
			return true;
		} catch( std::out_of_range ex_ ) {
			return false;
		}
	};

	bool Plugin::_releasePendingUpdate( const std::string& reference_, std::string& data_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_releasePendingUpdate( reference_, dummy, data_ );
	};

	bool Plugin::_releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_ ) {
		std::string dummy = "";
		return this->_releasePendingUpdate( reference_, source_, dummy );
	};

	bool Plugin::_releasePendingUpdate( const std::string& reference_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_releasePendingUpdate( reference_, dummy );
	};

} // namespace micasa
