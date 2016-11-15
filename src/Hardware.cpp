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

	extern std::shared_ptr<WebServer> g_webServer;

	Hardware::Hardware( std::string id_, std::map<std::string, std::string> settings_ ) : m_id( id_ ), m_settings( settings_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Hardware instances." );
#endif // _DEBUG
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Hardware instances." );
#endif // _DEBUG
	};

	std::shared_ptr<Hardware> Hardware::get( std::string id_, HardwareType hardwareType_, std::map<std::string, std::string> settings_ ) {
		switch( hardwareType_ ) {
			case INTERNAL:
				return std::make_shared<Internal>( id_, settings_ );
				break;
			case HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, settings_ );
				break;
			case OPEN_ZWAVE:
				return std::make_shared<OpenZWave>( id_, settings_ );
				break;
			case OPEN_ZWAVE_NODE:
				return std::make_shared<OpenZWaveNode>( id_, settings_ );
				break;
			case P1_METER:
				return std::make_shared<P1Meter>( id_, settings_ );
				break;
			case PIFACE:
				return std::make_shared<PiFace>( id_, settings_ );
				break;
			case PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, settings_ );
				break;
			case RFXCOM:
				return std::make_shared<RFXCom>( id_, settings_ );
				break;
			case SOLAREDGE:
				return std::make_shared<SolarEdge>( id_, settings_ );
				break;
			case WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, settings_ );
				break;
			case DOMOTICZ:
				return std::make_shared<Domoticz>( id_, settings_ );
				break;
		}
	}

	bool Hardware::start() {
		g_webServer->addResourceHandler( "hardware/" + this->m_id, WebServerResource::Method::GET | WebServerResource::Method::PUT | WebServerResource::Method::PATCH | WebServerResource::Method::DELETE, this->shared_from_this() );
		g_webServer->addResourceHandler( "hardware/" + this->m_id + "/devices", WebServerResource::Method::GET | WebServerResource::Method::HEAD, this->shared_from_this() );
		return true;
	};
	
	bool Hardware::stop() {
		g_webServer->removeResourceHandler( "hardware/" + this->m_id );
		g_webServer->removeResourceHandler( "hardware/" + this->m_id + "/devices" );
		return true;
	};

	
} // namespace micasa
