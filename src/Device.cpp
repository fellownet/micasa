#include "Device.h"

#include "Hardware.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	Device::Device( std::shared_ptr<Hardware> hardware_, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : m_hardware( hardware_ ), m_id( id_ ), m_unit( unit_ ), m_name( name_ ), m_settings( settings_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Device instances." );
		assert( g_webServer && "Global Database instance should be created before Device instances." );
		assert( g_logger && "Global Logger instance should be created before Device instances." );
#endif // _DEBUG
	};
	
	Device::~Device() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Device instances." );
		assert( g_webServer && "Global Database instance should be destroyed after Device instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Device instances." );
#endif // _DEBUG
	};

	std::shared_ptr<Device> Device::factory( std::shared_ptr<Hardware> hardware_, DeviceType deviceType_, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) {
		switch( deviceType_ ) {
			case COUNTER:
				return std::make_shared<Counter>( hardware_, id_, unit_, name_, settings_ );
				break;
			case LEVEL:
				return std::make_shared<Level>( hardware_, id_, unit_, name_, settings_ );
				break;
			case SWITCH:
				return std::make_shared<Switch>( hardware_, id_, unit_, name_, settings_ );
				break;
			case TEXT:
				return std::make_shared<Text>( hardware_, id_, unit_, name_, settings_ );
				break;
		}
	}

}; // namespace micasa
