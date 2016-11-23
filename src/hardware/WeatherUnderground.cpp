#include <sstream>

#include "json.hpp"

#include "WeatherUnderground.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Counter.h"
#include "../device/Switch.h"
#include "../Logger.h"

extern "C" {
	
	void wu_mg_handler( mg_connection* connection_, int event_, void* data_ ) {
		micasa::WeatherUnderground* hardware = reinterpret_cast<micasa::WeatherUnderground*>( connection_->user_data );
		if ( event_ == MG_EV_HTTP_REPLY ) {
			hardware->_processHttpReply( connection_, (http_message*)data_ );
		} else if ( event_ == MG_EV_CONNECT ) {
			mg_set_timer( connection_, mg_time() + 5 );
		} else if ( event_ == MG_EV_TIMER ) {
			hardware->_processHttpReply( connection_, NULL );
		}
	}
	
} // extern "C"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;

	using namespace nlohmann;

	std::string WeatherUnderground::toString() const {
		return this->m_name;
	};

	void WeatherUnderground::start() {
		g_webServer->addResource( {
			"api/hardware/" + this->m_id + "/forecast_url",
			WebServer::ResourceMethod::GET,
			"Retrieve the forecast url for hardware <i>" + this->m_name + "</i>.",
			this->shared_from_this()
		} );
		this->_begin();
		Hardware::start();
	}
	
	void WeatherUnderground::stop() {
		g_webServer->removeResourceAt( "api/hardware/" + this->m_id + "/forecast_url" );
		this->m_busy = false;
		this->_retire();
		Hardware::stop();
	}
	
	std::chrono::milliseconds WeatherUnderground::_work( const unsigned long int iteration_ ) {
		
		// Make sure all the required settings are present.
		if ( ! this->m_settings.contains( { "api_key", "location", "scale" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "http://api.wunderground.com/api/" << this->m_settings["api_key"] << "/conditions/q/" << this->m_settings["location"] << ".json";
#ifdef _DEBUG
		g_logger->logr( Logger::LogLevel::DEBUG, this, "Fetching from %s.", url.str().c_str() );
#endif // _DEBUG

		mg_mgr manager;
		mg_mgr_init( &manager, NULL );

		mg_connection* connection = mg_connect_http( &manager, wu_mg_handler, url.str().c_str(), "Accept: application/json\r\n", NULL );
		if ( NULL != connection ) {
			connection->user_data = this;
			mg_set_timer( connection, mg_time() + 5 );
			this->m_busy = true;
			while( this->m_busy ) {
				mg_mgr_poll( &manager, 1000 );
			}
		} else {
			g_logger->log( Logger::LogLevel::ERROR, this, "Unable to connect." );
		}
		
		mg_mgr_free( &manager );
		
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	};

	void WeatherUnderground::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		Hardware::handleResource( resource_, code_, output_ );
		if ( resource_.uri == "api/hardware/" + this->m_id + "/forecast_url" ) {
			output_["uri"] = "http://www.nu.nl";
		}
	};

	void WeatherUnderground::_processHttpReply( mg_connection* connection_, const http_message* message_ ) {
		if ( message_ == NULL ) {
			g_logger->log( Logger::LogLevel::WARNING, this, "Timeout when connecting." );
		} else {
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
		}
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
		this->m_busy = false;
	}
	
}; // namespace micasa
