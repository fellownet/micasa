#include <sstream>

#include "json.hpp"

#include "WeatherUnderground.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Counter.h"
#include "../device/Switch.h"
#include "../Logger.h"
#include "../Network.h"
#include "../Utils.h"
#include "../User.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Network> g_network;

	using namespace nlohmann;

	WeatherUnderground::WeatherUnderground( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		// The settings for WeatherUnderground need to be entered before the hardware is started. Therefore the
		// resource handler needs to be installed upon construction time. The resource will be destroyed by
		// the controller which uses the same identifier for specific hardware resources.
		g_webServer->addResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "$",
			99,
			User::Rights::INSTALLER,
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				auto settings = extractSettingsFromJson( input_ );
				try {
					this->m_settings->put( "api_key", settings.at( "api_key" ) );
				} catch( std::out_of_range exception_ ) { };
				try {
					this->m_settings->put( "location", settings.at( "location" ) );
				} catch( std::out_of_range exception_ ) { };
				try {
					this->m_settings->put( "scale", settings.at( "scale" ) );
				} catch( std::out_of_range exception_ ) { };
				if ( this->m_settings->isDirty() ) {
					this->m_settings->commit();
					this->m_needsRestart = true;
				}
			} )
		} );
	};

	void WeatherUnderground::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	};
	
	void WeatherUnderground::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	};

	json WeatherUnderground::getJson( bool full_ ) const {
		if ( full_ ) {
			json result = Hardware::getJson( full_ );
			result["settings"] = {
				{
					{ "name", "api_key" },
					{ "label", "API Key" },
					{ "type", "string" },
					{ "value", this->m_settings->get( "api_key", "" ) }
				},
				{
					{ "name", "location" },
					{ "label", "Location" },
					{ "type", "string" },
					{ "value", this->m_settings->get( "location", "" ) }
				},
				{
					{ "name", "scale" },
					{ "label", "Scale" },
					{ "type", "list" },
					{ "options", {
						{
							{ "value", "celcius" },
							{ "label", "Celcius" }
						},
						{
							{ "value", "fahrenheit" },
							{ "label", "Fahrenheid" }
						}
					} },
					{ "value", this->m_settings->get( "scale", "celcius" ) }
				}
			};
			return result;
		} else {
			return Hardware::getJson( full_ );
		}
	};
	
	std::chrono::milliseconds WeatherUnderground::_work( const unsigned long int& iteration_ ) {
		
		if ( ! this->m_settings->contains( { "api_key", "location", "scale" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "http://api.wunderground.com/api/" << this->m_settings->get( "api_key" ) << "/conditions/q/" << this->m_settings->get( "location" ) << ".json";

		g_network->connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REPLY ) {
				this->_processHttpReply( connection_, (http_message*)data_ );
			}  else if (
				event_ == MG_EV_CLOSE
				&& this->getState() == Hardware::State::INIT
			) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Connection failure." );
				this->setState( Hardware::State::FAILED );
			}
		} ) );
		
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	};

	void WeatherUnderground::_processHttpReply( mg_connection* connection_, const http_message* message_ ) {
		std::string body;
		body.assign( message_->body.p, message_->body.len );
		
		try {
			json data = json::parse( body );

			if ( data["response"].is_object() ) {
				if ( data["response"]["error"].is_null() ) {
					if (
						! data["current_observation"].is_null()
						&& data["current_observation"].is_object()
					) {
						data = data["current_observation"];
						
						Device::UpdateSource source = Device::UpdateSource::HARDWARE;
						if ( this->m_first ) {
							source |= Device::UpdateSource::INIT;
							this->m_first = false;
						}
						
						if (
							this->m_settings->get( "scale" ) == "fahrenheit"
							&& ! data["temp_f"].is_null()
						) {
							auto device = this->_declareDevice<Level>( "1", "Temperature in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::TEMPERATURE ) },
								{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::FAHRENHEIT ) }
							} );
							device->updateValue( source, data["temp_f"].get<double>() );
						} else if (
						   this->m_settings->get( "scale" ) == "celcius"
						   && ! data["temp_c"].is_null()
					   ) {
							auto device = this->_declareDevice<Level>( "2", "Temperature in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::TEMPERATURE ) },
								{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::CELCIUS ) }
							} );
							device->updateValue( source, data["temp_c"].get<double>() );
						}
						
						if ( ! data["relative_humidity"].is_null() ) {
							auto device = this->_declareDevice<Level>( "3", "Humidity in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::HUMIDITY ) },
								{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::PERCENT ) }
							} );
							device->updateValue( source, std::stod( data["relative_humidity"].get<std::string>() ) );
						}
						if ( ! data["pressure_mb"].is_null() ) {
							auto device = this->_declareDevice<Level>( "4", "Barometric pressure in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::PRESSURE ) },
								{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::PASCAL ) }
							} );
							device->updateValue( source, std::stod( data["pressure_mb"].get<std::string>() ) );
						}
						if ( ! data["wind_dir"].is_null() ) {
							auto device = this->_declareDevice<Text>( "5", "Wind Direction in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE, Text::resolveSubType( Text::SubType::WIND_DIRECTION ) }
							} );
							device->updateValue( source, data["wind_dir"].get<std::string>() );
						}
					}

					this->setState( Hardware::State::READY );

				} else {
					if (
						data["response"]["error"].is_object()
						&& ! data["response"]["error"]["description"].is_null()
					) {
						this->setState( Hardware::State::FAILED );
						g_logger->log( Logger::LogLevel::ERROR, this, data["response"]["error"]["description"].get<std::string>() );
					}
				}
			}
		} catch( ... ) {
			this->setState( Hardware::State::FAILED );
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	};
	
}; // namespace micasa
