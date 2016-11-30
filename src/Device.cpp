#include "Device.h"

#include "Hardware.h"
#include "Database.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	Device::Device( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ ) : m_hardware( hardware_ ), m_id( id_ ), m_reference( reference_ ), m_name( name_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Device instances." );
		assert( g_webServer && "Global Database instance should be created before Device instances." );
		assert( g_logger && "Global Logger instance should be created before Device instances." );
#endif // _DEBUG
		this->m_settings.populate( *this );
	};
	
	Device::~Device() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Device instances." );
		assert( g_webServer && "Global Database instance should be destroyed after Device instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Device instances." );
#endif // _DEBUG
	};

	void Device::start() {
		Worker::start();
	};
	
	void Device::stop() {
		Worker::stop();
	};
	
	std::shared_ptr<Device> Device::_factory( std::shared_ptr<Hardware> hardware_, const DeviceType deviceType_, const std::string id_, const std::string reference_, std::string name_ ) {
		switch( deviceType_ ) {
			case COUNTER:
				return std::make_shared<Counter>( hardware_, id_, reference_, name_ );
				break;
			case LEVEL:
				return std::make_shared<Level>( hardware_, id_, reference_, name_ );
				break;
			case SWITCH:
				return std::make_shared<Switch>( hardware_, id_, reference_, name_ );
				break;
			case TEXT:
				return std::make_shared<Text>( hardware_, id_, reference_, name_ );
				break;
		}
		return nullptr;
	};

}; // namespace micasa
