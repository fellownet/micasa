#include <iostream>
#include <sstream>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Hardware.h"

#include "Logger.h"
#include "Database.h"
#include "Controller.h"

#include "device/Level.h"
#include "device/Counter.h"
#include "device/Text.h"
#include "device/Switch.h"

#include "hardware/WeatherUnderground.h"
#include "hardware/HarmonyHub.h"
#include "hardware/RFXCom.h"
#include "hardware/SolarEdge.h"
#include "hardware/SolarEdgeInverter.h"
#include "hardware/Dummy.h"
#include "hardware/Telegram.h"
#ifdef _WITH_OPENZWAVE
	#include "hardware/ZWave.h"
	#include "hardware/ZWaveNode.h"
#endif // _WITH_OPENZWAVE
#ifdef _WITH_LINUX_SPI
	#include "hardware/PiFace.h"
	#include "hardware/PiFaceBoard.h"
#endif // _WITH_LINUX_SPI

namespace micasa {

	using namespace std::chrono;
	using namespace nlohmann;

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;

	const char* Hardware::settingsName = "hardware";

	const std::map<Hardware::Type, std::string> Hardware::TypeText = {
		{ Hardware::Type::HARMONY_HUB, "harmony_hub" },
		{ Hardware::Type::RFXCOM, "rfxcom" },
		{ Hardware::Type::SOLAREDGE, "solaredge" },
		{ Hardware::Type::SOLAREDGE_INVERTER, "solaredge_inverter" },
		{ Hardware::Type::WEATHER_UNDERGROUND, "weather_underground" },
		{ Hardware::Type::DUMMY, "dummy" },
		{ Hardware::Type::TELEGRAM, "telegram" },
#ifdef _WITH_OPENZWAVE
		{ Hardware::Type::ZWAVE, "zwave" },
		{ Hardware::Type::ZWAVE_NODE, "zwave_node" },
#endif // _WITH_OPENZWAVE
#ifdef _WITH_LINUX_SPI
		{ Hardware::Type::PIFACE, "piface" },
		{ Hardware::Type::PIFACE_BOARD, "piface_board" }
#endif // _WITH_LINUX_SPI
	};

	const std::map<Hardware::State, std::string> Hardware::StateText = {
		{ Hardware::State::INIT, "initializing" },
		{ Hardware::State::READY, "ready" },
		{ Hardware::State::DISABLED, "disabled" },
		{ Hardware::State::FAILED, "failed" },
		{ Hardware::State::SLEEPING, "sleeping" },
		{ Hardware::State::DISCONNECTED, "disconnected" }
	};

	Hardware::Hardware( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : m_id( id_ ), m_type( type_ ), m_reference( reference_ ), m_parent( parent_ ) {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before Hardware instances." );
#endif // _DEBUG
		this->m_settings = std::make_shared<Settings<Hardware>>( *this );
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after Hardware instances." );
		assert( this->m_state == Hardware::State::DISABLED && "Hardware should be stopped before being destructed." );
#endif // _DEBUG
		std::lock_guard<std::mutex> devicesLock( this->m_devicesMutex );
		this->m_devices.clear();
	};

	std::shared_ptr<Hardware> Hardware::factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) {
		switch( type_ ) {
			case Type::HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, type_, reference_, parent_ );
				break;
#ifdef _WITH_OPENZWAVE
			case Type::ZWAVE:
				return std::make_shared<ZWave>( id_, type_, reference_, parent_ );
				break;
			case Type::ZWAVE_NODE:
				return std::make_shared<ZWaveNode>( id_, type_, reference_, parent_ );
				break;
#endif // _WITH_OPENZWAVE
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
			case Type::WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, type_, reference_, parent_ );
				break;
			case Type::DUMMY:
				return std::make_shared<Dummy>( id_, type_, reference_, parent_ );
				break;
			case Type::TELEGRAM:
				return std::make_shared<Telegram>( id_, type_, reference_, parent_ );
				break;
		}
		return nullptr;
	}

	void Hardware::init() {
		std::lock_guard<std::mutex> devicesLock( this->m_devicesMutex );
		std::vector<std::map<std::string, std::string>> devicesData = g_database->getQuery(
			"SELECT `id`, `reference`, `label`, `type`, `enabled` "
			"FROM `devices` "
			"WHERE `hardware_id`=%d",
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

	void Hardware::start() {
		std::unique_lock<std::mutex> devicesLock( this->m_devicesMutex );
		for ( auto const &deviceIt : this->m_devices ) {
			if ( deviceIt.second->isEnabled() ) {
				deviceIt.second->start();
			}
		}
		devicesLock.unlock();

		// Set the state to initializing if the hardware itself didn't already set the state to something else.
		if ( this->getState() == State::DISABLED ) {
			this->setState( Hardware::State::INIT );
		}

		Logger::log( this->m_parent == nullptr ? Logger::LogLevel::NORMAL : Logger::LogLevel::VERBOSE, this, "Started." );
	};

	void Hardware::stop() {
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

	std::string Hardware::getName() const {
		return this->m_settings->get( "name", this->getLabel() );
	};

	void Hardware::setState( const State& state_, bool children_ ) {
		this->m_state = state_;
		if ( children_ ) {
			for ( auto& child : g_controller->getChildrenOfHardware( *this ) ) {
				child->setState( state_ );
			}
		}
	};

	json Hardware::getJson( bool full_ ) const {
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "enabled", this->getState() != Hardware::State::DISABLED },
			{ "type", Hardware::resolveTextType( this->m_type ) },
			{ "state", Hardware::resolveTextState( this->m_state ) }
		};
		if ( this->m_parent ) {
			result["parent"] = this->m_parent->getJson( false );
		}
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json Hardware::getSettingsJson() const {
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

		// Child hardware is enabled and disabled by it's parent.
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

	json Hardware::getDeviceJson( std::shared_ptr<const Device> device_, bool full_ ) const {
		return json::object();
	};

	json Hardware::getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const {
		return json::array();
	};

	std::shared_ptr<Device> Hardware::getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		try {
			return this->m_devices.at( reference_ );
		} catch( std::out_of_range ex_ ) {
			return nullptr;
		}
	};

	std::shared_ptr<Device> Hardware::getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getId() == id_ ) {
				return devicesIt.second;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getName() == name_ ) {
				return devicesIt.second;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getLabel() == label_ ) {
				return devicesIt.second;
			}
		}
		return nullptr;
	};

	std::vector<std::shared_ptr<Device>> Hardware::getAllDevices() const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		std::vector<std::shared_ptr<Device>> all;
		for ( auto const &devicesIt : this->m_devices ) {
			all.push_back( devicesIt.second );
		}
		return all;
	};

	std::vector<std::shared_ptr<Device>> Hardware::getAllDevices( const std::string& prefix_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		std::vector<std::shared_ptr<Device>> result;
		for ( auto const &devicesIt : this->m_devices ) {
			if ( devicesIt.second->getReference().substr( 0, prefix_.size() ) == prefix_ ) {
				result.push_back( devicesIt.second );
			}
		}
		return result;
	}

	void Hardware::removeDevice( const std::shared_ptr<Device> device_ ) {
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
				this->m_devices.erase( devicesIt );
				break;
			}
		}
	};

	template<class T> std::shared_ptr<T> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ ) {
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
			"INSERT INTO `devices` ( `hardware_id`, `reference`, `type`, `label`, `enabled` ) "
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

		return device;

	};
	template std::shared_ptr<Counter> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
	template std::shared_ptr<Level> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
	template std::shared_ptr<Switch> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
	template std::shared_ptr<Text> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto pendingUpdate = this->m_pendingUpdates[reference_];
		if ( __likely(
			pendingUpdate == nullptr
			|| pendingUpdate->waitFor( blockNewUpdate_ )
		) ) {
			this->m_pendingUpdates[reference_] = this->m_scheduler.schedule<t_pendingUpdate>( waitForResult_, 1, this, [this,reference_,source_,data_]( Scheduler::Task<t_pendingUpdate>& ) -> t_pendingUpdate {
				std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
				this->m_pendingUpdates.erase( reference_ );
				return { source_, data_ };
			} );
			return true;
		} else {
			return false;
		}
	};

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_queuePendingUpdate( reference_, dummy, data_, blockNewUpdate_, waitForResult_ );
	};

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		std::string dummy = "";
		return this->_queuePendingUpdate( reference_, source_, dummy, blockNewUpdate_, waitForResult_ );
	};

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_queuePendingUpdate( reference_, dummy, blockNewUpdate_, waitForResult_ );
	};

	bool Hardware::_releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_, std::string& data_ ) {
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

	bool Hardware::_releasePendingUpdate( const std::string& reference_, std::string& data_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_releasePendingUpdate( reference_, dummy, data_ );
	};

	bool Hardware::_releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_ ) {
		std::string dummy = "";
		return this->_releasePendingUpdate( reference_, source_, dummy );
	};

	bool Hardware::_releasePendingUpdate( const std::string& reference_ ) {
		Device::UpdateSource dummy = Device::resolveUpdateSource( 0 );
		return this->_releasePendingUpdate( reference_, dummy );
	};

} // namespace micasa
