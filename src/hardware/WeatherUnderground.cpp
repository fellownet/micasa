#include <sstream>

#include "json.hpp"

#include "WeatherUnderground.h"
#include "../Curl.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../Logger.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;

#ifdef _DEBUG
#define MULTILINE(...) #__VA_ARGS__
	const char *sample = MULTILINE(
		{
		  "response": {
		  "version":"0.1",
		  "termsofService":"http://www.wunderground.com/weather/api/d/terms.html",
		  "features": {
		  "conditions": 1
		  }
			}
		  ,	"current_observation": {
				"image": {
				"url":"http://icons.wxug.com/graphics/wu2/logo_130x80.png",
				"title":"Weather Underground",
				"link":"http://www.wunderground.com"
				},
				"display_location": {
				"full":"Venray, Netherlands",
				"city":"Venray",
				"state":"LI",
				"state_name":"Netherlands",
				"country":"NL",
				"country_iso3166":"NL",
				"zip":"00000",
				"magic":"20",
				"wmo":"KEDLV",
				"latitude":"51.52999878",
				"longitude":"5.98000002",
				"elevation":"29.0"
				},
				"observation_location": {
				"full":"De Brabander, Venray, LIMBURG",
				"city":"De Brabander, Venray",
				"state":"LIMBURG",
				"country":"NL",
				"country_iso3166":"NL",
				"latitude":"51.536533",
				"longitude":"5.965635",
				"elevation":"69 ft"
				},
				"estimated": {
				},
				"station_id":"ILIMBURG174",
				"observation_time":"Last Updated on November 8, 11:03 PM CET",
				"observation_time_rfc822":"Tue, 08 Nov 2016 23:03:09 +0100",
				"observation_epoch":"1478642589",
				"local_time_rfc822":"Tue, 08 Nov 2016 23:03:18 +0100",
				"local_epoch":"1478642598",
				"local_tz_short":"CET",
				"local_tz_long":"Europe/Amsterdam",
				"local_tz_offset":"+0100",
				"weather":"Clear",
				"temperature_string":"37.4 F (3.0 C)",
				"temp_f":37.4,
				"temp_c":3.0,
				"relative_humidity":"92%",
				"wind_string":"Calm",
				"wind_dir":"NW",
				"wind_degrees":310,
				"wind_mph":0.2,
				"wind_gust_mph":"2.5",
				"wind_kph":0,
				"wind_gust_kph":"4.0",
				"pressure_mb":"1007",
				"pressure_in":"29.74",
				"pressure_trend":"0",
				"dewpoint_string":"35 F (2 C)",
				"dewpoint_f":35,
				"dewpoint_c":2,
				"heat_index_string":"NA",
				"heat_index_f":"NA",
				"heat_index_c":"NA",
				"windchill_string":"37 F (3 C)",
				"windchill_f":"37",
				"windchill_c":"3",
				"feelslike_string":"37 F (3 C)",
				"feelslike_f":"37",
				"feelslike_c":"3",
				"visibility_mi":"N/A",
				"visibility_km":"N/A",
				"solarradiation":"0",
				"UV":"0.0","precip_1hr_string":"0.00 in ( 0 mm)",
				"precip_1hr_in":"0.00",
				"precip_1hr_metric":" 0",
				"precip_today_string":" in ( mm)",
				"precip_today_in":"",
				"precip_today_metric":"--",
				"icon":"clear",
				"icon_url":"http://icons.wxug.com/i/c/k/nt_clear.gif",
				"forecast_url":"http://www.wunderground.com/global/stations/KEDLV.html",
				"history_url":"http://www.wunderground.com/weatherstation/WXDailyHistory.asp?ID=ILIMBURG174",
				"ob_url":"http://www.wunderground.com/cgi-bin/findweather/getForecast?query=51.536533,5.965635",
				"nowcast":""
			}
		}
	);
#endif // _DEBUG

	using namespace nlohmann;

	WeatherUnderground::WeatherUnderground( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ), Worker() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before WeatherUnderground instances." );
		assert( g_logger && "Global Logger instance should be created before WeatherUnderground instances." );
#endif // _DEBUG
	};

	WeatherUnderground::~WeatherUnderground() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after WeatherUnderground instances." );
		assert( g_logger && "Global Logger instance should be destroyed after WeatherUnderground instances." );
#endif // _DEBUG
	};

	std::string WeatherUnderground::toString() const {
		return this->m_name;
	};

	void WeatherUnderground::start() {
		g_webServer->addResourceHandler( "hardware/" + this->m_id + "/forecast_url", WebServerResource::Method::GET, this->shared_from_this() );
		Hardware::start();
		this->_begin();
	}
	
	void WeatherUnderground::stop() {
		g_webServer->removeResourceHandler( "hardware/" + this->m_id + "/forecast_url" );
		Hardware::stop();
		this->_retire();
	}
	
	std::chrono::milliseconds WeatherUnderground::_work( unsigned long int iteration_ ) {
		if (
			this->m_settings.find( "api_key" ) == this->m_settings.end()
			|| this->m_settings.find( "location" ) == this->m_settings.end()
		) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "http://api.wunderground.com/api/" << this->m_settings.at( "api_key" ) << "/conditions/q/" << this->m_settings.at( "location" ) << ".json";

		try {
			Curl curl( url.str() );

#ifdef _DEBUG
			json jsonData = json::parse( sample );
#else
			json jsonData = json::parse( curl.fetch() );
#endif // _DEBUG

			if ( jsonData["response"].empty() ) {
				throw std::invalid_argument( "invalid syntax" );
			}
			if (
				! jsonData["response"].is_object()
				|| ! jsonData["response"]["error"].empty()
			) {
				if (
					jsonData["response"]["error"].is_object()
					&& ! jsonData["response"]["error"]["description"].empty()
				) {
					throw std::invalid_argument( jsonData["response"]["error"]["description"].get<std::string>() );
				} else {
					throw std::invalid_argument( "unknown error" );
				}
			}
			if (
				jsonData["current_observation"].empty()
				|| ! jsonData["current_observation"].is_object()
			) {
				throw std::invalid_argument( "missing observation" );
			}

			jsonData = jsonData["current_observation"];

			// Report celcius or fahrenheit.
			if (
				this->m_settings.at( "scale" ) == "fahrenheit"
				&& ! jsonData["temp_f"].empty()
			) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->declareDevice( Device::DeviceType::LEVEL, "1", "Temperature Fahrenheit", { } ) );
				device->updateValue( jsonData["temp_f"].get<float>() );
			} else if (
				this->m_settings.at( "scale" ) == "celcius"
				&& ! jsonData["temp_c"].empty()
			) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->declareDevice( Device::DeviceType::LEVEL, "2", "Temperature Degrees", { } ) );
				device->updateValue( jsonData["temp_c"].get<float>() );
			}

			if ( ! jsonData["relative_humidity"].empty() ) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->declareDevice( Device::DeviceType::LEVEL, "3", "Humidity", { } ) );
				int humidity = atoi( jsonData["relative_humidity"].get<std::string>().c_str() );
				device->updateValue( humidity );
			}
			if ( ! jsonData["pressure_mb"].empty() ) {
				std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->declareDevice( Device::DeviceType::LEVEL, "4", "Pressure", { } ) );
				int pressure = atoi( jsonData["pressure_mb"].get<std::string>().c_str() );
				device->updateValue( pressure );
			}
			if ( ! jsonData["wind_dir"].empty() ) {
				std::shared_ptr<Text> device = std::static_pointer_cast<Text>( this->declareDevice( Device::DeviceType::TEXT, "5", "Wind Direction", { } ) );
				device->updateValue( jsonData["wind_dir"].get<std::string>() );
			}

		} catch( CurlException exception_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Error fetching data (%s).", exception_.what() );
		} catch( std::invalid_argument invalidArgumentException_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response (%s).", invalidArgumentException_.what() );
		} catch( std::domain_error domainException_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Unexpected response (%s).", domainException_.what() );
		}

		g_logger->log( Logger::LogLevel::WARNING, this, "ding" );
		
#ifdef _DEBUG
		return std::chrono::milliseconds( 1000 * 60 );
#else
		return std::chrono::milliseconds( 1000 * 60 * 5 );
#endif // _DEBUG
	};

}; // namespace micasa
