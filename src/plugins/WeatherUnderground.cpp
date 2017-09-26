// https://www.wunderground.com/weather/api/d/docs?d=index

#include <sstream>

#include "WeatherUnderground.h"

#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Switch.h"
#include "../Logger.h"
#include "../Network.h"
#include "../Utils.h"

#include "json.hpp"

namespace micasa {

	using namespace nlohmann;

	const char* WeatherUnderground::label = "Weather Underground";

	WeatherUnderground::WeatherUnderground( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) :
		Plugin( id_, type_, reference_, parent_ ),
		m_details( "" )
	{
	};

	void WeatherUnderground::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();

		this->m_scheduler.schedule( 0, SCHEDULER_INTERVAL_5MIN, SCHEDULER_REPEAT_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			if ( ! this->m_settings->contains( { "api_key", "location", "scale" } ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Plugin::State::FAILED );
				return;
			}

			if ( this->m_connection != nullptr ) {
				this->m_connection->terminate();
			}

			std::stringstream url;
			url << "http://api.wunderground.com/api/" << this->m_settings->get( "api_key" ) << "/conditions/astronomy/q/" << this->m_settings->get( "location" ) << ".json";
			this->m_connection = Network::connect( url.str(), {}, [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) {
				switch( event_ ) {
					case Network::Connection::Event::CONNECT: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );
						break;
					}
					case Network::Connection::Event::FAILURE: {
						Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
						this->setState( Plugin::State::FAILED );
						break;
					}
					case Network::Connection::Event::HTTP: {
						this->_process( connection_->getBody() );
						break;
					}
					case Network::Connection::Event::DROPPED:
					case Network::Connection::Event::CLOSE: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connection closed." );
						break;
					}
					default: { break; }
				}
			} );
		} );
	};

	void WeatherUnderground::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		if ( this->m_connection != nullptr ) {
			this->m_connection->terminate();
		}
		Plugin::stop();
	};

	std::string WeatherUnderground::getLabel() const {
		if ( this->m_details.size() ) {
			std::stringstream label;
			label << WeatherUnderground::label << " (" << this->m_details << ")";
			return label.str();
		} else {
			return WeatherUnderground::label;
		}
	};

	json WeatherUnderground::getJson() const {
		json result = Plugin::getJson();
		result["api_key"] = this->m_settings->get( "api_key", "" );
		result["location"] = this->m_settings->get( "location", "" );
		result["scale"] = this->m_settings->get( "scale", "celsius" );
		return result;
	};

	json WeatherUnderground::getSettingsJson() const {
		json result = Plugin::getSettingsJson();
		for ( auto &&setting : WeatherUnderground::getEmptySettingsJson( true ) ) {
			result.push_back( setting );
		}
		return result;
	};

	json WeatherUnderground::getEmptySettingsJson( bool advanced_ ) {
		json result = json::array();
		result += {
			{ "name", "api_key" },
			{ "label", "API Key" },
			{ "type", "string" },
			{ "mandatory", true },
			{ "class", advanced_ ? "advanced" : "normal" },
			{ "sort", 97 }
		};
		result += {
			{ "name", "location" },
			{ "label", "Location" },
			{ "type", "string" },
			{ "mandatory", true },
			{ "class", advanced_ ? "advanced" : "normal" },
			{ "sort", 98 }
		};
		result += {
			{ "name", "scale" },
			{ "label", "Scale" },
			{ "type", "list" },
			{ "mandatory", true },
			{ "class", advanced_ ? "advanced" : "normal" },
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

	void WeatherUnderground::_process( const std::string& data_ ) {
		try {
			json data = json::parse( data_ );

			if ( data["response"].is_object() ) {
				if ( data["response"]["error"].is_null() ) {
					this->setState( Plugin::State::READY );

					if (
						! data["current_observation"].is_null()
						&& data["current_observation"].is_object()
					) {
						auto observation = data["current_observation"];

						if ( this->m_settings->get( "scale" ) == "fahrenheit" ) {
							this->declareDevice<Level>( "1", "Temperature in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::TEMPERATURE ) },
								{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::FAHRENHEIT ) }
							} )->updateValue( Device::UpdateSource::PLUGIN, jsonGet<double>( observation, "temp_f" ) );
						} else if ( this->m_settings->get( "scale" ) == "celsius" ) {
							this->declareDevice<Level>( "2", "Temperature in " + this->m_settings->get( "location" ), {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::TEMPERATURE ) },
								{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::CELSIUS ) }
							} )->updateValue( Device::UpdateSource::PLUGIN, jsonGet<double>( observation, "temp_c" ) );
						}
						this->declareDevice<Level>( "3", "Humidity in " + this->m_settings->get( "location" ), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::HUMIDITY ) },
							{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::PERCENT ) }
						} )->updateValue( Device::UpdateSource::PLUGIN, jsonGet<double>( observation, "relative_humidity" ) );
						this->declareDevice<Level>( "4", "Barometric pressure in " + this->m_settings->get( "location" ), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::PRESSURE ) },
							{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::PASCAL ) }
						} )->updateValue( Device::UpdateSource::PLUGIN, jsonGet<double>( observation, "pressure_mb" ) );
						this->declareDevice<Text>( "5", "Wind Direction in " + this->m_settings->get( "location" ), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveTextSubType( Text::SubType::WIND_DIRECTION ) }
						} )->updateValue( Device::UpdateSource::PLUGIN, jsonGet<std::string>( observation, "wind_dir" ) );
					}

					if (
						! data["moon_phase"].is_null()
						&& data["moon_phase"].is_object()
					) {
						auto moonphase = data["moon_phase"];

						double now = jsonGet<unsigned int>( moonphase["current_time"], "hour" ) + ( jsonGet<unsigned int>( moonphase["current_time"], "minute" ) / 100.0f );
						double sunrise = jsonGet<unsigned int>( moonphase["sunrise"], "hour" ) + ( jsonGet<unsigned int>( moonphase["sunrise"], "minute" ) / 100.0f );
						double sunset = jsonGet<unsigned int>( moonphase["sunset"], "hour" ) + ( jsonGet<unsigned int>( moonphase["sunset"], "minute" ) / 100.0f );

						char buffer[50];
						int length = sprintf( buffer, "sunrise %5.2fh sunset %5.2fh", sunrise, sunset );
						this->m_details = this->m_settings->get( "location" ) + ", " + std::string( buffer, length );

						Switch::Option value;
						if (
							sunrise < now
							&& sunset > now
						) {
							value = Switch::Option::ON;
						} else {
							value = Switch::Option::OFF;
						}

						this->declareDevice<Switch>( "6", "Daytime in " + this->m_settings->get( "location" ), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) }
						} )->updateValue( Device::UpdateSource::PLUGIN, value );
						this->declareDevice<Level>( "7", "Sunrise in " + this->m_settings->get( "location" ), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) }
						} )->updateValue( Device::UpdateSource::PLUGIN, sunrise );
						this->declareDevice<Level>( "8", "Sunset in " + this->m_settings->get( "location" ), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) }
						} )->updateValue( Device::UpdateSource::PLUGIN, sunset );
					}

					this->m_scheduler.schedule( 1000 * 10, 1, NULL, [this]( std::shared_ptr<Scheduler::Task<>> ) -> void {
						this->setState( Plugin::State::SLEEPING );
					} );
				} else {
					if (
						data["response"]["error"].is_object()
						&& ! data["response"]["error"]["description"].is_null()
					) {
						this->setState( Plugin::State::FAILED );
						Logger::log( Logger::LogLevel::ERROR, this, data["response"]["error"]["description"].get<std::string>() );
					}
				}
			}
		} catch( json::exception ex_ ) {
			this->setState( Plugin::State::FAILED );
			Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
		}
	};

}; // namespace micasa
