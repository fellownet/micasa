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
		this->_begin();
		Hardware::start();
	}
	
	void WeatherUnderground::stop() {
		this->_retire();
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

			if ( data["response"].empty() ) {
				throw std::invalid_argument( "invalid syntax" );
			}
			if (
				! data["response"].is_object()
				|| ! data["response"]["error"].empty()
			) {
				if (
					data["response"]["error"].is_object()
					&& ! data["response"]["error"]["description"].empty()
				) {
					throw std::invalid_argument( data["response"]["error"]["description"].get<std::string>() );
				} else {
					throw std::invalid_argument( "unknown error" );
				}
			}
			if (
				data["current_observation"].empty()
				|| ! data["current_observation"].is_object()
			) {
				throw std::invalid_argument( "missing observation" );
			}
			
			data = data["current_observation"];
			
			// Report celcius or fahrenheit.
			if (
				this->m_settings["scale"] == "fahrenheit"
				&& ! data["temp_f"].empty()
			) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, "1", "Temperature in " + this->m_settings["location"], { } ) );
				device->updateValue( Device::UpdateSource::HARDWARE, data["temp_f"].get<float>() );
			} else if (
			   this->m_settings["scale"] == "celcius"
			   && ! data["temp_c"].empty()
		   ) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, "2", "Temperature in " + this->m_settings["location"], { } ) );
				device->updateValue( Device::UpdateSource::HARDWARE, data["temp_c"].get<float>() );
			}
			
			if ( ! data["relative_humidity"].empty() ) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, "3", "Humidity in " + this->m_settings["location"], { } ) );
				int humidity = atoi( data["relative_humidity"].get<std::string>().c_str() );
				device->updateValue( Device::UpdateSource::HARDWARE, humidity );
			}
			
			if ( ! data["pressure_mb"].empty() ) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, "4", "Barometric pressure in " + this->m_settings["location"], { } ) );
				int pressure = atoi( data["pressure_mb"].get<std::string>().c_str() );
				device->updateValue( Device::UpdateSource::HARDWARE, pressure );
			}
			
			if ( ! data["wind_dir"].empty() ) {
				std::shared_ptr<Text> device = std::static_pointer_cast<Text>( this->_declareDevice( Device::DeviceType::TEXT, "5", "Wind Direction in " + this->m_settings["location"], { } ) );
				device->updateValue( Device::UpdateSource::HARDWARE, data["wind_dir"].get<std::string>() );
			}

		} catch( std::invalid_argument invalidArgumentException_ ) {
			g_logger->logr( Logger::LogLevel::ERROR, this, "Invalid response (%s).", invalidArgumentException_.what() );
		} catch( std::domain_error domainException_ ) {
			g_logger->logr( Logger::LogLevel::ERROR, this, "Unexpected response (%s).", domainException_.what() );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	}
	
}; // namespace micasa
