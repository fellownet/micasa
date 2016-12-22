#include "Dummy.h"
#include "../Database.h"

#include "../device/Counter.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Switch.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Database> g_database;
	
	void Dummy::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		this->_installResourceHandlers();
		this->_setState( READY );
		Hardware::start();
	}
	
	void Dummy::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		g_webServer->removeResourceCallback( "dummy-" + std::to_string( this->m_id ) );
		Hardware::stop();
	}

	void Dummy::_installResourceHandlers() {

		// Add resource handlers for creating or deleting dummy devices.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"dummy-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ),
			WebServer::Method::POST | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::POST: {
						
						unsigned int index = g_database->getQueryValue<unsigned int>(
							"SELECT MAX(`reference`) "
							"FROM `devices` "
							"WHERE `hardware_id`=%d"
							, this->m_id
						);
						
						try {
							if (
								input_.find( "type" ) != input_.end()
								&& input_.find( "name" ) != input_.end()
							) {
								if ( input_["type"].get<std::string>() == "counter" ) {
									auto device = this->_declareDevice<Counter>( std::to_string( ++index ), "Counter", {
										{ "name", input_["name"].get<std::string>() },
										{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) }
									} );
									device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, 0 );
								} else if ( input_["type"].get<std::string>() == "level" ) {
									auto device = this->_declareDevice<Level>( std::to_string( ++index ), "Level", {
										{ "name", input_["name"].get<std::string>() },
										{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) }
									} );
									device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, 0. );
								} else if ( input_["type"].get<std::string>() == "text" ) {
									auto device = this->_declareDevice<Text>( std::to_string( ++index ), "Text", {
										{ "name", input_["name"].get<std::string>() },
										{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) }
									} );
									device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, "" );
								} else if ( input_["type"].get<std::string>() == "switch" ) {
									auto device = this->_declareDevice<Switch>( std::to_string( ++index ), "Switch", {
										{ "name", input_["name"].get<std::string>() },
										{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) }
									} );
									device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::OFF );
								}
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing fields.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to create dummy device.";
							code_ = 400; // bad request
						}
						break;
					}
					case WebServer::Method::DELETE: {
						break;
					}
					default: break;
				}
			} )
		} ) ) );
	};

}; // namespace micasa
