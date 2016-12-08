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

	void WeatherUnderground::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	}
	
	void WeatherUnderground::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	}
	
	std::chrono::milliseconds WeatherUnderground::_work( const unsigned long int iteration_ ) {
		
		if ( ! this->m_settings.contains( { "api_key", "location", "scale" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "http://api.wunderground.com/api/" << this->m_settings["api_key"] << "/conditions/q/" << this->m_settings["location"] << ".json";

		g_network->connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REPLY ) {
				this->_processHttpReply( connection_, (http_message*)data_ );
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
						
						if (
							this->m_settings["scale"] == "fahrenheit"
							&& ! data["temp_f"].is_null()
						) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, "1", "Temperature in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::DEGREES ) }
							} ) );
							device->updateValue( Device::UpdateSource::HARDWARE, data["temp_f"].get<float>() );
						} else if (
						   this->m_settings["scale"] == "celcius"
						   && ! data["temp_c"].is_null()
					   ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, "2", "Temperature in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::DEGREES ) }
							} ) );
							device->updateValue( Device::UpdateSource::HARDWARE, data["temp_c"].get<float>() );
						}
						
						if ( ! data["relative_humidity"].is_null() ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, "3", "Humidity in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::PERCENT ) }
							} ) );
							device->updateValue( Device::UpdateSource::HARDWARE, data["relative_humidity"].get<int>() );
						}
						
						if ( ! data["pressure_mb"].is_null() ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, "4", "Barometric pressure in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
								{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::PASCAL ) }
							} ) );
							device->updateValue( Device::UpdateSource::HARDWARE, data["pressure_mb"].get<int>() );
						}
						
						if ( ! data["wind_dir"].is_null() ) {
							std::shared_ptr<Text> device = std::static_pointer_cast<Text>( this->_declareDevice( Device::Type::TEXT, "5", "Wind Direction in " + this->m_settings["location"], {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
							} ) );
							device->updateValue( Device::UpdateSource::HARDWARE, data["wind_dir"].get<std::string>() );
						}
					}
				} else {
					if (
						data["response"]["error"].is_object()
						&& ! data["response"]["error"]["description"].is_null()
					) {
						g_logger->log( Logger::LogLevel::ERROR, this, data["response"]["error"]["description"].get<std::string>() );
					}
				}
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	}
	
}; // namespace micasa
