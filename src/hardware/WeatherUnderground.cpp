#include <sstream>

#include "WeatherUnderground.h"

#include "../device/Level.h"
#include "../device/Text.h"
#include "../Logger.h"
#include "../Network.h"

#include "json.hpp"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Network> g_network;

	using namespace nlohmann;

	void WeatherUnderground::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	};
	
	void WeatherUnderground::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	};

	json WeatherUnderground::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["api_key"] = this->m_settings->get( "api_key", "" );
		result["location"] = this->m_settings->get( "location", "" );
		result["scale"] = this->m_settings->get( "scale", "celsius" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json WeatherUnderground::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		result += {
			{ "name", "api_key" },
			{ "label", "API Key" },
			{ "type", "string" },
			{ "class", this->m_settings->contains( "api_key" ) ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 97 }
		};
		result += {
			{ "name", "location" },
			{ "label", "Location" },
			{ "type", "string" },
			{ "mandatory", true },
			{ "sort", 98 }
		};
		result += {
			{ "name", "scale" },
			{ "label", "Scale" },
			{ "type", "list" },
			{ "class", this->m_settings->contains( "scale" ) ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 },
			{ "options", {
				{
					{ "value", "celsius" },
					{ "label", "Celsius" }
				},
				{
					{ "value", "fahrenheit" },
					{ "label", "Fahrenheid" }
				}
			} }
		};
		return result;
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
				std::string body;
				body.assign( ((http_message*)data_)->body.p, ((http_message*)data_)->body.len );
				this->_processHttpReply( body );
				connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
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

	void WeatherUnderground::_processHttpReply( const std::string& body_ ) {
		try {
			json data = json::parse( body_ );

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
							auto device = this->declareDevice<Level>( "1", "Temperature in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::TEMPERATURE ) },
								{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::FAHRENHEIT ) }
							} );
							device->updateValue( source, data["temp_f"].get<double>() );
						} else if (
						   this->m_settings->get( "scale" ) == "celsius"
						   && ! data["temp_c"].is_null()
					   ) {
							auto device = this->declareDevice<Level>( "2", "Temperature in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::TEMPERATURE ) },
								{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::CELSIUS ) }
							} );
							device->updateValue( source, data["temp_c"].get<double>() );
						}
						
						if ( ! data["relative_humidity"].is_null() ) {
							auto device = this->declareDevice<Level>( "3", "Humidity in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::HUMIDITY ) },
								{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::PERCENT ) }
							} );
							device->updateValue( source, std::stod( data["relative_humidity"].get<std::string>() ) );
						}
						if ( ! data["pressure_mb"].is_null() ) {
							auto device = this->declareDevice<Level>( "4", "Barometric pressure in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::PRESSURE ) },
								{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::PASCAL ) }
							} );
							device->updateValue( source, std::stod( data["pressure_mb"].get<std::string>() ) );
						}
						if ( ! data["wind_dir"].is_null() ) {
							auto device = this->declareDevice<Text>( "5", "Wind Direction in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveSubType( Text::SubType::WIND_DIRECTION ) }
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
	};
	
}; // namespace micasa
