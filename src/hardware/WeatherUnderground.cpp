#include <sstream>

#include "json.hpp"

#include "WeatherUnderground.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Counter.h"
#include "../device/Switch.h"
#include "../Logger.h"
#include "../Network.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Network> g_network;

	using namespace nlohmann;

	WeatherUnderground::WeatherUnderground( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		this->m_settings.put<std::string>( HARDWARE_SETTINGS_ALLOWED, "api_key,location,scale" );
	};

	void WeatherUnderground::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	};
	
	void WeatherUnderground::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	};
	
	const std::chrono::milliseconds WeatherUnderground::_work( const unsigned long int& iteration_ ) {
		
		if ( ! this->m_settings.contains( { "api_key", "location", "scale" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->_setState( Hardware::State::FAILED );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "http://api.wunderground.com/api/" << this->m_settings["api_key"] << "/conditions/q/" << this->m_settings["location"] << ".json";

		g_network->connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REPLY ) {
				this->_processHttpReply( connection_, (http_message*)data_ );
			}  else if (
				event_ == MG_EV_CLOSE
				&& this->getState() == Hardware::State::INIT
			) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Connection failure." );
				this->_setState( Hardware::State::FAILED );
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
						
						unsigned int source = Device::UpdateSource::HARDWARE;
						if ( this->m_first ) {
							source |= Device::UpdateSource::INIT;
							this->m_first = false;
						}
						
						if (
							this->m_settings["scale"] == "fahrenheit"
							&& ! data["temp_f"].is_null()
						) {
							auto device = this->_declareDevice<Level>( "1", "Temperature in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::FAHRENHEIT ) }
							} );
							device->updateValue( source, data["temp_f"].get<double>() );
						} else if (
						   this->m_settings["scale"] == "celcius"
						   && ! data["temp_c"].is_null()
					   ) {
							auto device = this->_declareDevice<Level>( "2", "Temperature in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::CELCIUS ) }
							} );
							device->updateValue( source, data["temp_c"].get<double>() );
						}
						
						if ( ! data["relative_humidity"].is_null() ) {
							auto device = this->_declareDevice<Level>( "3", "Humidity in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::PERCENT ) }
							} );
							device->updateValue( source, std::stod( data["relative_humidity"].get<std::string>() ) );
						}
						if ( ! data["pressure_mb"].is_null() ) {
							auto device = this->_declareDevice<Level>( "4", "Barometric pressure in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::PASCAL ) }
							} );
							device->updateValue( source, std::stod( data["pressure_mb"].get<std::string>() ) );
						}
						if ( ! data["wind_dir"].is_null() ) {
							auto device = this->_declareDevice<Text>( "5", "Wind Direction in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
							} );
							device->updateValue( source, data["wind_dir"].get<std::string>() );
						}
					}

					this->_setState( Hardware::State::READY );

				} else {
					if (
						data["response"]["error"].is_object()
						&& ! data["response"]["error"]["description"].is_null()
					) {
						this->_setState( Hardware::State::FAILED );
						g_logger->log( Logger::LogLevel::ERROR, this, data["response"]["error"]["description"].get<std::string>() );
					}
				}
			}
		} catch( ... ) {
			this->_setState( Hardware::State::FAILED );
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	};
	
}; // namespace micasa
