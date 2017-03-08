#include <iostream>
#include <sstream>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Hardware.h"

#include "Logger.h"
#include "Database.h"

#include "device/Level.h"
#include "device/Counter.h"
#include "device/Text.h"
#include "device/Switch.h"

#ifdef _WITH_OPENZWAVE
	#include "hardware/ZWave.h"
	#include "hardware/ZWaveNode.h"
#endif // _WITH_OPENZWAVE
#include "hardware/WeatherUnderground.h"
#include "hardware/PiFace.h"
#include "hardware/PiFaceBoard.h"
#include "hardware/HarmonyHub.h"
#include "hardware/P1Meter.h"
#include "hardware/RFXCom.h"
#include "hardware/SolarEdge.h"
#include "hardware/SolarEdgeInverter.h"
#include "hardware/Dummy.h"
#include "hardware/Telegram.h"

namespace micasa {

	using namespace nlohmann;

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	const std::map<Hardware::Type, std::string> Hardware::TypeText = {
		{ Hardware::Type::HARMONY_HUB, "harmony_hub" },
#ifdef _WITH_OPENZWAVE
		{ Hardware::Type::ZWAVE, "zwave" },
		{ Hardware::Type::ZWAVE_NODE, "zwave_node" },
#endif // _WITH_OPENZWAVE
#ifdef _WITH_LINUX_SPI
		{ Hardware::Type::PIFACE, "piface" },
		{ Hardware::Type::PIFACE_BOARD, "piface_board" },
#endif // _WITH_LINUX_SPI
		{ Hardware::Type::P1_METER, "p1_meter" },
		{ Hardware::Type::RFXCOM, "rfxcom" },
		{ Hardware::Type::SOLAREDGE, "solaredge" },
		{ Hardware::Type::SOLAREDGE_INVERTER, "solaredge_inverter" },
		{ Hardware::Type::WEATHER_UNDERGROUND, "weather_underground" },
		{ Hardware::Type::DUMMY, "dummy" },
		{ Hardware::Type::TELEGRAM, "telegram" }
	};

	const std::map<Hardware::State, std::string> Hardware::StateText = {
		{ Hardware::State::INIT, "initializing" },
		{ Hardware::State::READY, "ready" },
		{ Hardware::State::DISABLED, "disabled" },
		{ Hardware::State::FAILED, "failed" },
		{ Hardware::State::SLEEPING, "sleeping" },
		{ Hardware::State::DISCONNECTED, "disconnected" }
	};

	Hardware::Hardware( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Worker(), m_id( id_ ), m_type( type_ ), m_reference( reference_ ), m_parent( parent_ ) {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before Hardware instances." );
		assert( g_logger && "Global Logger instance should be created before Hardware instances." );
#endif // _DEBUG
		this->m_settings = std::make_shared<Settings<Hardware> >( *this );
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after Hardware instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Hardware instances." );
#endif // _DEBUG
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
			case Type::P1_METER:
				return std::make_shared<P1Meter>( id_, type_, reference_, parent_ );
				break;
			case Type::PIFACE:
				return std::make_shared<PiFace>( id_, type_, reference_, parent_ );
				break;
			case Type::PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, type_, reference_, parent_ );
				break;
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

	void Hardware::start() {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );

		// Fetch all device records from the database, including those that are disabled. Disabled devices are *not*
		// started but are still present in the devices vector.
		std::vector<std::map<std::string, std::string> > devicesData = g_database->getQuery(
			"SELECT `id`, `reference`, `label`, `type`, `enabled` "
			"FROM `devices` "
			"WHERE `hardware_id`=%d"
			, this->m_id
		);
		for ( auto devicesIt = devicesData.begin(); devicesIt != devicesData.end(); devicesIt++ ) {
			Device::Type type = Device::resolveType( (*devicesIt)["type"] );
			std::shared_ptr<Device> device = Device::factory( this->shared_from_this(), type, std::stoi( (*devicesIt)["id"] ), (*devicesIt)["reference"], (*devicesIt)["label"] );

			if ( (*devicesIt)["enabled"] == "1" ) {
				device->start();
			}

			this->m_devices.push_back( device );
		}

		// Set the state to initializing if the hardware itself didn't already set the state to something else.
		if ( this->getState() == State::DISABLED ) {
			this->setState( INIT );
		}
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Hardware::stop() {
		{
			std::lock_guard<std::mutex> lock( this->m_devicesMutex );
			for( auto devicesIt = this->m_devices.begin(); devicesIt < this->m_devices.end(); devicesIt++ ) {
				auto device = (*devicesIt);
				if ( device->isRunning() ) {
					device->stop();
				}
			}
			this->m_devices.clear();
		}

		if ( this->m_settings->isDirty() ) {
			this->m_settings->commit();
		}

		this->setState( State::DISABLED );
		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::string Hardware::getName() const {
		return this->m_settings->get( "name", this->getLabel() );
	};

	void Hardware::setState( const State& state_ ) {
		if ( this->m_state != state_ ) {
			this->m_state = state_;
		}
	};

	json Hardware::getJson( bool full_ ) const {
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "enabled", this->isRunning() },
			{ "type", Hardware::resolveType( this->m_type ) },
			{ "state", Hardware::resolveState( this->m_state ) },
			{ "last_update", g_database->getQueryValue<unsigned long>(
				"SELECT CAST(strftime('%%s',`updated`) AS INTEGER) "
				"FROM `hardware` "
				"WHERE `id`=%d ",
				this->getId()
			) }
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

		setting = {
			{ "name", "enabled" },
			{ "label", "Enabled" },
			{ "type", "boolean" },
			{ "default", false },
			{ "sort", 2 }
		};
		result += setting;

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
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getId() == id_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getName() == name_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getLabel() == label_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::vector<std::shared_ptr<Device> > Hardware::getAllDevices() const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		return std::vector<std::shared_ptr<Device> >( this->m_devices );
	};

	std::vector<std::shared_ptr<Device> > Hardware::getAllDevices( const std::string& prefix_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		std::vector<std::shared_ptr<Device> > result;
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference().substr( 0, prefix_.size() ) == prefix_ ) {
				result.push_back( *devicesIt );
			}
		}
		return result;
	}

	void Hardware::removeDevice( const std::shared_ptr<Device> device_ ) {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt) == device_ ) {

				if ( device_->isRunning() ) {
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

	void Hardware::touch() {
		if ( this->m_parent ) {
			g_database->putQuery(
				"UPDATE `hardware` "
				"SET `updated`=datetime('now') "
				"WHERE `id`=%d "
				"OR `id`=%d ",
				this->getId(),
				this->m_parent->getId()
			);
		} else {
			g_database->putQuery(
				"UPDATE `hardware` "
				"SET `updated`=datetime('now') "
				"WHERE `id`=%d ",
				this->getId()
			);
		}
	};

	template<class T> std::shared_ptr<T> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ ) {
		// TODO also declare relationships with other devices, such as energy and power, or temperature
		// and humidity. Provide a hardcoded list of references upon declaring so that these relationships
		// can be altered at will by the client (maybe they want temperature and pressure).
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				std::shared_ptr<T> device = std::static_pointer_cast<T>( *devicesIt );

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
			}
		}

		long id = g_database->putQuery(
			"INSERT INTO `devices` ( `hardware_id`, `reference`, `type`, `label`, `enabled` ) "
			"VALUES ( %d, %Q, %Q, %Q, %d )",
			this->m_id,
			reference_.c_str(),
			Device::resolveType( T::type ).c_str(),
			label_.c_str(),
			start_ ? 1 : 0
		);
		std::shared_ptr<T> device = std::static_pointer_cast<T>( Device::factory( this->shared_from_this(), T::type, id, reference_, label_ ) );

		auto settings = device->getSettings();
		settings->insert( settings_ );
		if ( settings->isDirty() ) {
			settings->commit();
		}

		if ( start_ ) {
			device->start();
		}

		this->m_devices.push_back( device );

		return device;

	};
	template std::shared_ptr<Counter> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );
	template std::shared_ptr<Level> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );
	template std::shared_ptr<Switch> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );
	template std::shared_ptr<Text> Hardware::declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		
		// See if there's already a pending update present, in which case we need to use it to start locking.
		std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		
		auto search = this->m_pendingUpdates.find( reference_ );
		if ( search == this->m_pendingUpdates.end() ) {
			this->m_pendingUpdates[reference_] = std::make_shared<PendingUpdate>( source_, data_ );
		}
		std::shared_ptr<PendingUpdate> pendingUpdate = this->m_pendingUpdates[reference_];
		pendingUpdatesLock.unlock();

		if ( pendingUpdate->updateMutex.try_lock_for( std::chrono::milliseconds( blockNewUpdate_ ) ) ) {
			std::thread( [this,pendingUpdate,reference_,waitForResult_] {

				std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
				pendingUpdate->condition.wait_for( notifyLock, std::chrono::milliseconds( waitForResult_ ), [&pendingUpdate]{ return pendingUpdate->done; } );
				
				// Spurious wakeups are someting we have to live with; there's no way to determine if the amount
				// of time was passed or if a spurious wakeup occured.

				std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
				auto search = this->m_pendingUpdates.find( reference_ );
				if ( search != this->m_pendingUpdates.end() ) {
					this->m_pendingUpdates.erase( search );
				}
				pendingUpdatesLock.unlock();

				pendingUpdate->updateMutex.unlock();
			} ).detach();
			return true;
		} else {
			return false;
		}
	};

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		std::string dummy;
		return this->_queuePendingUpdate( reference_, source_, dummy, blockNewUpdate_, waitForResult_ );
	};

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		Device::UpdateSource dummy;
		return this->_queuePendingUpdate( reference_, dummy, blockNewUpdate_, waitForResult_ );
	};

	bool Hardware::_releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_, std::string& data_ ) {
		std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( reference_ );
		if ( search != this->m_pendingUpdates.end() ) {
			auto pendingUpdate = search->second;
			std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
			pendingUpdate->done = true;
			notifyLock.unlock();
			pendingUpdate->condition.notify_all();
			source_ |= pendingUpdate->source;
			data_ = pendingUpdate->data;
			return true;
		}
		return false;
	};

	bool Hardware::_releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_ ) {
		std::string dummy;
		return this->_releasePendingUpdate( reference_, source_, dummy );
	};

	bool Hardware::_releasePendingUpdate( const std::string& reference_ ) {
		Device::UpdateSource dummy;
		return this->_releasePendingUpdate( reference_, dummy );
	};

	bool Hardware::_hasPendingUpdate( const std::string& reference_ ) {
		std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( reference_ );
		return search != this->m_pendingUpdates.end();
	};

} // namespace micasa
