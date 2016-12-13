#include <iostream>

#include "Hardware.h"
#include "Database.h"
#include "Controller.h"

#include "hardware/OpenZWave.h"
#include "hardware/OpenZWaveNode.h"
#include "hardware/WeatherUnderground.h"
#include "hardware/PiFace.h"
#include "hardware/PiFaceBoard.h"
#include "hardware/HarmonyHub.h"
#include "hardware/P1Meter.h"
#include "hardware/RFXCom.h"
#include "hardware/SolarEdge.h"
#include "hardware/SolarEdgeInverter.h"
 
namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	const std::map<Hardware::Type, std::string> Hardware::TypeText = {
		{ Hardware::Type::HARMONY_HUB, "Harmony Hub" },
		{ Hardware::Type::OPEN_ZWAVE, "OpenZWave" },
		{ Hardware::Type::OPEN_ZWAVE_NODE, "OpenZWave Node" },
		{ Hardware::Type::P1_METER, "P1 Meter" },
		{ Hardware::Type::PIFACE, "PiFace" },
		{ Hardware::Type::PIFACE_BOARD, "PiFace Board" },
		{ Hardware::Type::RFXCOM, "RFXCom" },
		{ Hardware::Type::SOLAREDGE, "SolarEdge API" },
		{ Hardware::Type::SOLAREDGE_INVERTER, "SolarEdge Inverter" },
		{ Hardware::Type::WEATHER_UNDERGROUND, "Weather Underground" }
	};

	const std::map<Hardware::Status, std::string> Hardware::StatusText = {
		{ Hardware::Status::INIT, "Initializing" },
		{ Hardware::Status::READY, "Ready" },
		{ Hardware::Status::DISABLED, "Disabled" },
		{ Hardware::Status::FAILED, "Failed" },
		{ Hardware::Status::SLEEPING, "Sleeping" }
	};
	
	Hardware::Hardware( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Worker(), m_id( id_ ), m_reference( reference_ ), m_parent( parent_ ), m_label( label_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Hardware instances." );
		assert( g_webServer && "Global Database instance should be created before Hardware instances." );
		assert( g_logger && "Global Logger instance should be created before Hardware instances." );
#endif // _DEBUG
		this->m_settings.populate( *this );
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Hardware instances." );
		assert( g_webServer && "Global Database instance should be destroyed after Hardware instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Hardware instances." );
#endif // _DEBUG
	};

	std::shared_ptr<Hardware> Hardware::_factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) {
		switch( type_ ) {
			case HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, reference_, parent_, label_ );
				break;
			case OPEN_ZWAVE:
				return std::make_shared<OpenZWave>( id_, reference_, parent_, label_ );
				break;
			case OPEN_ZWAVE_NODE:
				return std::make_shared<OpenZWaveNode>( id_, reference_, parent_, label_ );
				break;
			case P1_METER:
				return std::make_shared<P1Meter>( id_, reference_, parent_, label_ );
				break;
			case PIFACE:
				return std::make_shared<PiFace>( id_, reference_, parent_, label_ );
				break;
			case PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, reference_, parent_, label_ );
				break;
			case RFXCOM:
				return std::make_shared<RFXCom>( id_, reference_, parent_, label_ );
				break;
			case SOLAREDGE:
				return std::make_shared<SolarEdge>( id_, reference_, parent_, label_ );
				break;
			case SOLAREDGE_INVERTER:
				return std::make_shared<SolarEdgeInverter>( id_, reference_, parent_, label_ );
				break;
			case WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, reference_, parent_, label_ );
				break;
		}
#ifdef _DEBUG
		assert( true && "Hardware types should be defined in the Type enum." );
#endif // _DEBUG
		return nullptr;
	}

	void Hardware::start() {
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"api/hardware",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const std::string& uri_, const std::map<std::string, std::string>& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( output_.is_null() ) {
							output_ = nlohmann::json::array();
						}
						output_ += this->getJson();
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ),
			WebServer::Method::GET | WebServer::Method::DELETE | WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const std::string& uri_, const std::map<std::string, std::string>& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = this->getJson();
						break;
					}
					case WebServer::Method::DELETE: {
						g_controller->removeHardware( this->shared_from_this() );
						output_["result"] = "OK";
						break;
						
					}
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );

		std::vector<std::map<std::string, std::string> > devicesData = g_database->getQuery(
			"SELECT `id`, `reference`, `label`, `type` "
			"FROM `devices` "
			"WHERE `hardware_id`=%d"
			, this->m_id
		);
		for ( auto devicesIt = devicesData.begin(); devicesIt != devicesData.end(); devicesIt++ ) {
			Device::Type type = static_cast<Device::Type>( atoi( (*devicesIt)["type"].c_str() ) );
#ifdef _DEBUG
			assert( type >= 1 && type <= 4 && "Device types should be defined in the Type enum." );
#endif // _DEBUG
			std::shared_ptr<Device> device = Device::_factory( this->shared_from_this(), type, std::stoi( (*devicesIt)["id"] ), (*devicesIt)["reference"], (*devicesIt)["label"] );
			device->start();
			this->m_devices.push_back( device );
		}

		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void Hardware::stop() {
		g_webServer->removeResourceCallback( "hardware-" + std::to_string( this->m_id ) );
		
		{
			std::lock_guard<std::mutex> lock( this->m_devicesMutex );
			for( auto devicesIt = this->m_devices.begin(); devicesIt < this->m_devices.end(); devicesIt++ ) {
				(*devicesIt)->stop();
			}
			this->m_devices.clear();
		}

		if ( this->m_settings.isDirty() ) {
			this->m_settings.commit( *this );
		}
		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	const std::string Hardware::getName() const {
		return this->m_settings.get( "name", this->m_label );
	};
	
	void Hardware::setLabel( const std::string& label_ ) {
		if ( label_ != this->m_label ) {
			this->m_label = label_;
			g_database->putQuery(
				"UPDATE `hardware` "
				"SET `label`=%Q "
				"WHERE `id`=%d"
				, label_.c_str(), this->m_id
			);
		}
	};
	
	const nlohmann::json Hardware::getJson() const {
		nlohmann::json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() }
		};
		if ( this->m_parent ) {
			result["parent"] = this->m_parent->getJson();
		}
		return result;
	};
	
	std::shared_ptr<Device> Hardware::_getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};
	
	std::shared_ptr<Device> Hardware::_getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getId() == id_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::_getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getLabel() == label_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::_declareDevice( const Device::Type type_, const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_ ) {
		// TODO also declare relationships with other devices, such as energy and power, or temperature
		// and humidity. Provide a hardcoded list of references upon declaring so that these relationships
		// can be altered at will by the client (maybe they want temperature and pressure).
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				auto device = *devicesIt;
				
				// System settings (settings that start with an underscore and are usually defined by #define)
				// should always overwrite existing system settings.
				for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
					if ( settingsIt->first.at( 0 ) == '_' ) {
						device->getSettings().put( settingsIt->first, settingsIt->second );
					}
				}

				return device;
			}
		}

		long id = g_database->putQuery(
			"INSERT INTO `devices` ( `hardware_id`, `reference`, `type`, `label` ) "
			"VALUES ( %d, %Q, %d, %Q )"
			, this->m_id, reference_.c_str(), static_cast<int>( type_ ), label_.c_str()
		);
		std::shared_ptr<Device> device = Device::_factory( this->shared_from_this(), type_, id, reference_, label_ );

		Settings& settings = device->getSettings();
		settings.insert( settings_ );
		settings.commit( *device );
		
		device->start();
		this->m_devices.push_back( device );

		return device;
	};
	
	const bool Hardware::_queuePendingUpdate( const std::string& reference_, const unsigned int& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		// See if there's already a pending update present, in which case we need to use it to start locking.
		std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( reference_ );
		if ( search == this->m_pendingUpdates.end() ) {
			this->m_pendingUpdates[reference_] = std::make_shared<PendingUpdate>( source_ );
		}
		std::shared_ptr<PendingUpdate>& pendingUpdate = this->m_pendingUpdates[reference_];
		pendingUpdatesLock.unlock();
		
		if ( pendingUpdate->updateMutex.try_lock_for( std::chrono::milliseconds( blockNewUpdate_ ) ) ) {
			std::thread( [this,pendingUpdate,reference_,waitForResult_] { // pendingUpdate and valueId_ are copied into the thread
				
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
	
	const unsigned int Hardware::_releasePendingUpdate( const std::string& reference_ ) {
		std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( reference_ );
		if ( search != this->m_pendingUpdates.end() ) {
			auto pendingUpdate = search->second;
			std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
			pendingUpdate->done = true;
			notifyLock.unlock();
			pendingUpdate->condition.notify_all();
			return pendingUpdate->source;
		}
		return 0;
	};
	
} // namespace micasa
