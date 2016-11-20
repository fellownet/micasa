#include <iostream>

#include "Hardware.h"
#include "Database.h"

#include "hardware/Internal.h"
#include "hardware/OpenZWave.h"
#include "hardware/OpenZWaveNode.h"
#include "hardware/WeatherUnderground.h"
#include "hardware/PiFace.h"
#include "hardware/PiFaceBoard.h"
#include "hardware/HarmonyHub.h"
#include "hardware/P1Meter.h"
#include "hardware/RFXCom.h"
#include "hardware/SolarEdge.h"
#include "hardware/Domoticz.h"
 
namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	Hardware::Hardware( const std::string id_, const std::string reference_, std::string name_ ) : m_id( id_ ), m_reference( reference_ ), m_name( name_ ) {
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

	std::shared_ptr<Hardware> Hardware::_factory( HardwareType hardwareType_, std::string id_, std::string reference_, std::string name_ ) {
		switch( hardwareType_ ) {
			case INTERNAL:
				return std::make_shared<Internal>( id_, reference_, name_ );
				break;
			case HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, reference_, name_ );
				break;
			case OPEN_ZWAVE:
				return std::make_shared<OpenZWave>( id_, reference_, name_ );
				break;
			case OPEN_ZWAVE_NODE:
				return std::make_shared<OpenZWaveNode>( id_, reference_, name_ );
				break;
			case P1_METER:
				return std::make_shared<P1Meter>( id_, reference_, name_ );
				break;
			case PIFACE:
				return std::make_shared<PiFace>( id_, reference_, name_ );
				break;
			case PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, reference_, name_ );
				break;
			case RFXCOM:
				return std::make_shared<RFXCom>( id_, reference_, name_ );
				break;
			case SOLAREDGE:
				return std::make_shared<SolarEdge>( id_, reference_, name_ );
				break;
			case WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, reference_, name_ );
				break;
			case DOMOTICZ:
				return std::make_shared<Domoticz>( id_, reference_, name_ );
				break;
		}
#ifdef _DEBUG
		assert( true && "Hardware types should be defined in the HardwareType enum." );
#endif // _DEBUG
	}

	void Hardware::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		g_webServer->addResource( {
			"api/hardware/" + this->m_id,
			WebServer::ResourceMethod::GET | WebServer::ResourceMethod::PUT | WebServer::ResourceMethod::PATCH | WebServer::ResourceMethod::DELETE,
			"Retrieve the details of hardware <i>" + this->m_name + "</i>.",
			this->shared_from_this()
		} );
		g_webServer->addResource( {
			"api/hardware/" + this->m_id + "/devices",
			WebServer::ResourceMethod::GET | WebServer::ResourceMethod::GET | WebServer::ResourceMethod::HEAD,
			"Retrieve a list of devices belonging to hardware <i>" + this->m_name + "</i>.",
			this->shared_from_this()
		} );

		std::lock_guard<std::mutex> lock( this->m_devicesMutex );

		std::vector<std::map<std::string, std::string> > devicesData = g_database->getQuery(
			"SELECT `id`, `reference`, `name`, `type` "
			"FROM `devices` "
			"WHERE `hardware_id`=%q"
			, this->m_id.c_str()
		);
		for ( auto devicesIt = devicesData.begin(); devicesIt != devicesData.end(); devicesIt++ ) {
			Device::DeviceType deviceType = static_cast<Device::DeviceType>( atoi( (*devicesIt)["type"].c_str() ) );
#ifdef _DEBUG
			assert( deviceType >= 1 && deviceType <= 4 && "Device types should be defined in the DeviceType enum." );
#endif // _DEBUG
			std::shared_ptr<Device> device = Device::_factory( this->shared_from_this(), deviceType, (*devicesIt)["id"], (*devicesIt)["reference"], (*devicesIt)["name"] );
			device->start();
			this->m_devices.push_back( device );
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void Hardware::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		{
			std::lock_guard<std::mutex> lock( this->m_devicesMutex );
			for( auto devicesIt = this->m_devices.begin(); devicesIt < this->m_devices.end(); devicesIt++ ) {
				(*devicesIt)->stop();
			}
			this->m_devices.clear();
		}

		g_webServer->removeResourceAt( "api/hardware/" + this->m_id );
		g_webServer->removeResourceAt( "api/hardware/" + this->m_id + "/devices" );

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Device> Hardware::_declareDevice( Device::DeviceType deviceType_, std::string reference_, std::string name_, std::map<std::string, std::string> settings_ ) {
		// TODO also declare relationships with other devices, such as energy and power, or temperature
		// and humidity. Provide a hardcoded list of references upon declaring so that these relationships
		// can be altered at will by the client (maybe they want temperature and pressure).
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				return *devicesIt;
			}
		}
		
		long id = g_database->putQuery(
			"INSERT INTO `devices` ( `hardware_id`, `reference`, `type`, `name` ) "
			"VALUES ( %q, %Q, %d, %Q )"
			, this->m_id.c_str(), reference_.c_str(), static_cast<int>( deviceType_ ), name_.c_str()
		);
		std::shared_ptr<Device> device = Device::_factory( this->shared_from_this(), deviceType_, std::to_string( id ), reference_, name_ );
		
		Settings& settings = device->getSettings();
		settings.insert( settings_ );
		settings.commit( *device );
		
		device->start();
		this->m_devices.push_back( device );

		g_webServer->touchResourceAt( "api/hardware/" + this->m_id );
		g_webServer->touchResourceAt( "api/hardware/" + this->m_id + "/devices" );
		
		return device;
	};
	
	void Hardware::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		if ( resource_.uri == "api/hardware/" + this->m_id ) {
			output_ = g_database->getQuery(
				"SELECT `id`, `type`, `name` "
				"FROM `hardware`"
				"WHERE `id`=%q"
				, this->m_id.c_str()
			);
		} else if ( resource_.uri == "api/hardware/" + this->m_id + "/devices" ) {
			output_ = g_database->getQuery(
				"SELECT `id`, `hardware_id`, `type`, `name` "
				"FROM `devices`"
				"WHERE `hardware_id`=%q"
				, this->m_id.c_str()
			);
		}
	};
	
} // namespace micasa
