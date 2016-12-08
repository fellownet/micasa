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

	Device::Device( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : m_hardware( hardware_ ), m_id( id_ ), m_reference( reference_ ), m_label( label_ ) {
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

	std::ostream& operator<<( std::ostream& out_, const Device* device_ ) {
		out_ << device_->m_hardware->getName() << " / " << device_->getName(); return out_;
	}
	
	void Device::start() {
		Worker::start();
	};
	
	void Device::stop() {
		if ( this->m_settings.isDirty() ) {
			this->m_settings.commit( *this );
		}
		Worker::stop();
	};
	
	const std::string Device::getName() const {
		return this->m_settings.get( "name", this->m_label );
	};

	std::shared_ptr<Device> Device::_factory( std::shared_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_ ) {
		switch( type_ ) {
			case COUNTER:
				return std::make_shared<Counter>( hardware_, id_, reference_, label_ );
				break;
			case LEVEL:
				return std::make_shared<Level>( hardware_, id_, reference_, label_ );
				break;
			case SWITCH:
				return std::make_shared<Switch>( hardware_, id_, reference_, label_ );
				break;
			case TEXT:
				return std::make_shared<Text>( hardware_, id_, reference_, label_ );
				break;
		}
		return nullptr;
	};

	const nlohmann::json Device::getJson() const {
		nlohmann::json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "hardware", this->m_hardware->getJson() }
		};
		return result;
	};

}; // namespace micasa
