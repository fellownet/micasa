#include <iostream>

#include "Hardware.h"

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

	Hardware::Hardware( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : m_id( id_ ), m_unit( unit_ ), m_name( name_ ), m_settings( settings_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Hardware instances." );
		assert( g_webServer && "Global Database instance should be created before Hardware instances." );
		assert( g_logger && "Global Logger instance should be created before Hardware instances." );
#endif // _DEBUG
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Hardware instances." );
		assert( g_webServer && "Global Database instance should be destroyed after Hardware instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Hardware instances." );
#endif // _DEBUG
	};

	std::shared_ptr<Hardware> Hardware::factory( HardwareType hardwareType_, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) {
		switch( hardwareType_ ) {
			case INTERNAL:
				return std::make_shared<Internal>( id_, unit_, name_, settings_ );
				break;
			case HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, unit_, name_, settings_ );
				break;
			case OPEN_ZWAVE:
				return std::make_shared<OpenZWave>( id_, unit_, name_, settings_ );
				break;
			case OPEN_ZWAVE_NODE:
				return std::make_shared<OpenZWaveNode>( id_, unit_, name_, settings_ );
				break;
			case P1_METER:
				return std::make_shared<P1Meter>( id_, unit_, name_, settings_ );
				break;
			case PIFACE:
				return std::make_shared<PiFace>( id_, unit_, name_, settings_ );
				break;
			case PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, unit_, name_, settings_ );
				break;
			case RFXCOM:
				return std::make_shared<RFXCom>( id_, unit_, name_, settings_ );
				break;
			case SOLAREDGE:
				return std::make_shared<SolarEdge>( id_, unit_, name_, settings_ );
				break;
			case WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, unit_, name_, settings_ );
				break;
			case DOMOTICZ:
				return std::make_shared<Domoticz>( id_, unit_, name_, settings_ );
				break;
		}
	}

	void Hardware::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		std::lock_guard<std::mutex> lock( this->m_devicesMutex );

		g_webServer->addResourceHandler( "hardware/" + this->m_id, WebServerResource::Method::GET | WebServerResource::Method::PUT | WebServerResource::Method::PATCH | WebServerResource::Method::DELETE, this->shared_from_this() );
		g_webServer->addResourceHandler( "hardware/" + this->m_id + "/devices", WebServerResource::Method::GET | WebServerResource::Method::HEAD, this->shared_from_this() );

		std::vector<std::map<std::string, std::string> > devicesData = g_database->getQuery( "SELECT `id`, `unit`, `name`, `type` FROM `devices` WHERE `hardware_id`=%q", this->m_id.c_str() );
		for ( auto devicesIt = devicesData.begin(); devicesIt != devicesData.end(); devicesIt++ ) {
			Device::DeviceType deviceType = static_cast<Device::DeviceType>( atoi( (*devicesIt)["type"].c_str() ) );
			std::map<std::string, std::string> settings = g_database->getQueryMap( "SELECT `key`, `value` FROM `device_settings` WHERE `device_id`=%q", (*devicesIt).at( "id" ).c_str() );
		
			std::shared_ptr<Device> device = Device::factory( this->shared_from_this(), deviceType, (*devicesIt)["id"], (*devicesIt)["unit"], (*devicesIt)["name"], settings );

			g_webServer->addResourceHandler( "hardware/" + this->m_id + "/devices/" + device->getId(), WebServerResource::Method::GET | WebServerResource::Method::HEAD, device );
			g_webServer->addResourceHandler( "devices/" + device->getId(), WebServerResource::Method::GET | WebServerResource::Method::PUT | WebServerResource::Method::PATCH | WebServerResource::Method::DELETE, device );

			this->m_devices.push_back( device );
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void Hardware::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		{
			std::lock_guard<std::mutex> lock( this->m_devicesMutex );
			for( auto devicesIt = this->m_devices.begin(); devicesIt < this->m_devices.end(); devicesIt++ ) {
				g_webServer->removeResourceHandler( "hardware/" + this->m_id + "/devices/" + (*devicesIt)->getId() );
				g_webServer->removeResourceHandler( "devices/" + (*devicesIt)->getId() );
			}
			this->m_devices.clear();
		}

		g_webServer->removeResourceHandler( "hardware/" + this->m_id );
		g_webServer->removeResourceHandler( "hardware/" + this->m_id + "/devices" );

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Device> Hardware::declareDevice( Device::DeviceType deviceType_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getUnit() == unit_ ) {
				return *devicesIt;
			}
		}
		
		long id = g_database->putQuery( "INSERT INTO `devices` ( `hardware_id`, `unit`, `type`, `name` ) VALUES ( %q, %Q, %d, %Q )", this->m_id.c_str(), unit_.c_str(), static_cast<int>( deviceType_ ), name_.c_str() );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			g_database->putQuery( "INSERT INTO `device_settings` ( `device_id`, `key`, `value` ) VALUES ( %d, %Q, %Q )", id, settingsIt->first.c_str(), settingsIt->second.c_str() );
		}

		std::shared_ptr<Device> device = Device::factory( this->shared_from_this(), deviceType_, std::to_string( id ), unit_, name_, settings_ );

		g_webServer->addResourceHandler( "hardware/" + this->m_id + "/devices/" + device->getId(), WebServerResource::Method::GET | WebServerResource::Method::HEAD, device );
		g_webServer->addResourceHandler( "devices/" + device->getId(), WebServerResource::Method::GET | WebServerResource::Method::PUT | WebServerResource::Method::PATCH | WebServerResource::Method::DELETE, device );
		
		this->m_devices.push_back( device );
		
		return device;
	};
	
} // namespace micasa
