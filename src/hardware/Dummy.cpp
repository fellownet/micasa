#include "Dummy.h"
#include "../Database.h"
#include "../User.h"

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
		this->setState( READY );
		Hardware::start();
	}
	
	void Dummy::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		g_webServer->removeResourceCallback( "dummy-" + std::to_string( this->m_id ) );
		Hardware::stop();
	}

	void Dummy::_installResourceHandlers() {

		// Add resource handlers for creating dummy devices. Note that deleting devices is handled through
		// the general hardware delete resource handler.
		g_webServer->addResourceCallback( {
			"dummy-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "$",
			101,
			User::Rights::INSTALLER,
			WebServer::Method::POST,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::POST: {
						if ( input_.find( "type") == input_.end() ) {
							throw WebServer::ResourceException( { 400, "Device.Missing.Type", "Missing type." } );
						}
						if ( ! input_["type"].is_string() ) {
							throw WebServer::ResourceException( { 400, "Device.Invalid.Type", "The supplied type is invalid." } );
						}
						if ( input_.find( "name") == input_.end() ) {
							throw WebServer::ResourceException( { 400, "Device.Missing.Name", "Missing name." } );
						}
						if ( ! input_["name"].is_string() ) {
							throw WebServer::ResourceException( { 400, "Device.Invalid.Name", "The supplied name is invalid." } );
						}
					
						unsigned int index = g_database->getQueryValue<unsigned int>(
							"SELECT MAX(`reference`) "
							"FROM `devices` "
							"WHERE `hardware_id`=%d"
							, this->m_id
						);
						index++;

						if ( input_["type"].get<std::string>() == "counter" ) {
							auto device = this->_declareDevice<Counter>( std::to_string( index ), "Counter", {
								{ "name", input_["name"].get<std::string>() },
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
								{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true },
								{ DEVICE_SETTING_ALLOW_UNIT_CHANGE, true }
							}, true /* auto start */ );
							device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, 0 );
							output_["device"] = device->getJson();
						} else if ( input_["type"].get<std::string>() == "level" ) {
							auto device = this->_declareDevice<Level>( std::to_string( index ), "Level", {
								{ "name", input_["name"].get<std::string>() },
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
								{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true },
								{ DEVICE_SETTING_ALLOW_UNIT_CHANGE, true }
							}, true /* auto start */ );
							device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, 0. );
							output_["device"] = device->getJson();
						} else if ( input_["type"].get<std::string>() == "text" ) {
							auto device = this->_declareDevice<Text>( std::to_string( index ), "Text", {
								{ "name", input_["name"].get<std::string>() },
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
								{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true }
							}, true /* auto start */ );
							device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, "" );
							output_["device"] = device->getJson();
						} else if ( input_["type"].get<std::string>() == "switch" ) {
							auto device = this->_declareDevice<Switch>( std::to_string( index ), "Switch", {
								{ "name", input_["name"].get<std::string>() },
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
								{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true }
							}, true /* auto start */ );
							device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::OFF );
							output_["device"] = device->getJson();
						} else {
							throw WebServer::ResourceException( { 400, "Device.Invalid.Type", "The supplied type is invalid." } );
						}
						
						output_["code"] = 200;
						break;
					}
					default: break;
				}
			} )
		} );
	};

}; // namespace micasa
