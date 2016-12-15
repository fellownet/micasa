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
		this->m_lastUpdate = std::chrono::system_clock::now();
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
	};
	
	void Device::start() {
		// TODO move most resource callbacks from specific devices to here because they should be
		// mostly the same by now.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices/" + std::to_string( this->m_id ),
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {
						try {
							if (
								! input_["id"].is_null()
								&& input_["id"] == this->m_id
							) {
								if ( ! input_["name"].is_null() ) {
									this->m_settings.put( "name", input_["name"].get<std::string>() );
									this->m_settings.commit( *this );
								}
								g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "An id property is required for safety reasons.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to save device.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		
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

	void Device::setLabel( const std::string& label_ ) {
		if ( label_ != this->m_label ) {
			this->m_label = label_;
			g_database->putQuery(
				"UPDATE `devices` "
				"SET `label`=%Q "
				"WHERE `id`=%d"
				, label_.c_str(), this->m_id
			);
			g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
		}
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

	json Device::getJson() const {
		auto age = std::chrono::system_clock::now() - this->m_lastUpdate;
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "hardware", this->m_hardware->getJson() },
			// TODO the age on resources on the webpage doesn't update if the device doesn't, so lastUpdate might
			// be better somehow.
			{ "age", std::chrono::duration_cast<std::chrono::milliseconds>( age ).count() / 1000. }
		};
		return result;
	};

}; // namespace micasa
