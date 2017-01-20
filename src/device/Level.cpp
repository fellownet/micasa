#include "Level.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"
#include "../User.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	const Device::Type Level::type = Device::Type::LEVEL;

	const std::map<Level::SubType, std::string> Level::SubTypeText = {
		{ Level::SubType::GENERIC, "generic" },
		{ Level::SubType::TEMPERATURE, "temperature" },
		{ Level::SubType::HUMIDITY, "humidity" },
		{ Level::SubType::POWER, "power" },
		{ Level::SubType::ELECTRICITY, "electricity" },
		{ Level::SubType::CURRENT, "current" },
		{ Level::SubType::PRESSURE, "pressure" },
		{ Level::SubType::LUMINANCE, "luminance" },
		{ Level::SubType::THERMOSTAT_SETPOINT, "thermostat_setpoint" },
		{ Level::SubType::BATTERY_LEVEL, "battery_level" },
		{ Level::SubType::DIMMER, "dimmer" },
	};

	const std::map<Level::Unit, std::string> Level::UnitText = {
		{ Level::Unit::GENERIC, "" },
		{ Level::Unit::PERCENT, "%" },
		{ Level::Unit::WATT, "Watt" },
		{ Level::Unit::VOLT, "V" },
		{ Level::Unit::AMPERES, "A" },
		{ Level::Unit::CELCIUS, "°C" },
		{ Level::Unit::FAHRENHEIT, "°F" },
		{ Level::Unit::PASCAL, "Pa" },
		{ Level::Unit::LUX, "lx" },
	};
	
	void Level::start() {
		try {
			this->m_value = g_database->getQueryValue<double>(
				"SELECT `value` "
				"FROM `device_level_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1"
				, this->m_id
			);
		} catch( const Database::NoResultsException& ex_ ) { }
		
		g_webServer->addResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"^api/devices/" + std::to_string( this->m_id ) + "/data$",
			100,
			User::Rights::VIEWER,
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				// TODO check range to fetch
				output_["data"] = g_database->getQuery<json>(
					"SELECT * FROM ( "
						"SELECT printf(\"%%.3f\", `average`) AS `value`, CAST(strftime('%%s',`date`) AS INTEGER) AS `timestamp` "
						"FROM `device_level_trends` "
						"WHERE `device_id`=%d "
						"AND `date` < datetime('now','-%d day') "
						"ORDER BY `date` ASC "
					")"
					"UNION ALL "
					"SELECT * FROM ( "
						"SELECT printf(\"%%.3f\", avg(`value`)) AS `value`, strftime('%%s',`date`) - ( strftime('%%s',`date`) %% ( 5 * 60 ) ) AS `timestamp` "
						"FROM `device_level_history` "
						"WHERE `device_id`=%d "
						"AND `date` >= datetime('now','-%d day') "
						"GROUP BY `timestamp` "
						"ORDER BY `date` ASC "
					") ",
					this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 7 ), this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 7 )
				);
				output_["code"] = 200;
			} )
		} );

		Device::start();
	};
	
	void Level::stop() {
		Device::stop();
	};

	bool Level::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {

		// The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}

		// Do not process duplicate values.
		if ( this->m_value == value_ ) {
			return true;
		}

		// Make a local backup of the original value (the hardware might want to revert it).
		this->m_previousValue = this->m_value;
		this->m_value = value_;
		
		// If the update originates from the hardware, do not send it to the hardware again!
		bool success = true;
		bool apply = true;
		if ( ( source_ & Device::UpdateSource::HARDWARE ) != Device::UpdateSource::HARDWARE ) {
			success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		}
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_level_history` (`device_id`, `value`) "
				"VALUES (%d, %.3f)",
				this->m_id,
				value_
			);
			if ( this->isRunning() ) {
				g_controller->newEvent<Level>( *this, source_ );
			}
			this->m_lastUpdate = std::chrono::system_clock::now(); // after newEvent so the interval can be determined
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %.3f.", value_ );
		} else {
			this->m_value = this->m_previousValue;
		}
		return success;
	};

	json Level::getJson( bool full_ ) const {
		std::stringstream ss;
		ss << std::fixed << std::setprecision( 3 ) << this->getValue();
		json result = Device::getJson( full_ );
		result["value"] = ss.str();
		result["type"] = "level";
		result["subtype"] = Level::resolveSubType( this->m_settings->get<SubType>( "subtype", this->m_settings->get<SubType>( DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( "generic" ) ) ) );
		result["unit"] = Level::resolveUnit( this->m_settings->get<Unit>( "units", this->m_settings->get<Unit>( DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( "generic" ) ) ) );

		if ( full_ ) {
			if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, false ) ) {
				json setting = {
					{ "name", "subtype" },
					{ "label", "SubType" },
					{ "type", "list" },
					{ "options", json::array() },
					{ "value", result["subtype"] }
				};
				for ( auto subTypeIt = Level::SubTypeText.begin(); subTypeIt != Level::SubTypeText.end(); subTypeIt++ ) {
					setting["options"] += {
						{ "value", subTypeIt->second },
						{ "label", subTypeIt->second }
					};
				}
				result["settings"] += setting;
			}
			if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_UNIT_CHANGE, false ) ) {
				json setting = {
					{ "name", "unit" },
					{ "label", "Unit" },
					{ "type", "list" },
					{ "options", json::array() },
					{ "value", result["unit"] }
				};
				for ( auto unitIt = Level::UnitText.begin(); unitIt != Level::UnitText.end(); unitIt++ ) {
					setting["options"] += {
						{ "value", unitIt->second },
						{ "label", unitIt->second }
					};
				}
				result["settings"] += setting;
			}
		}

		return result;
	};

	std::chrono::milliseconds Level::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {
			std::string hourFormat = "%Y-%m-%d %H:00:00";
			std::string groupFormat = "%Y-%m-%d-%H";
			// TODO store value every 5 minutes regardless of updates?
			// TODO do not generate trends for anything older than an hour?? and what if micasa was offline for a while?
			// TODO maybe grab the latest hour from trends and start from there?
			auto trends = g_database->getQuery(
				"SELECT MAX(`value`) AS `max`, MIN(`value`) AS `min`, printf(\"%%.3f\", AVG(`value`)) AS `average`, strftime(%Q, MAX(`date`)) AS `date` "
				"FROM `device_level_history` "
				"WHERE `device_id`=%d AND `Date` > datetime('now','-1 hour') "
				"GROUP BY strftime(%Q, `date`)"
				, hourFormat.c_str(), this->m_id, groupFormat.c_str()
			);
			for ( auto trendsIt = trends.begin(); trendsIt != trends.end(); trendsIt++ ) {
				// TODO round these values to, say, 3 decimals?
				g_database->putQuery(
					"REPLACE INTO `device_level_trends` (`device_id`, `min`, `max`, `average`, `date`) "
					"VALUES (%d, %q, %q, %q, %Q)"
					, this->m_id, (*trendsIt)["min"].c_str(), (*trendsIt)["max"].c_str(), (*trendsIt)["average"].c_str(), (*trendsIt)["date"].c_str()
				);
			}
			// Purge history after a configured period (defaults to 7 days for level devices because these have a
			// separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_level_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 7 )
			);
			return std::chrono::milliseconds( 1000 * 60 * 5 );
		} else {
			// To prevent all devices from crunching data at the same time an offset is used.
			static volatile unsigned int offset = 0;
			offset += ( 1000 * 10 ); // 10 seconds interval
			return std::chrono::milliseconds( offset % ( 1000 * 60 * 5 ) );
		}
	};

}; // namespace micasa
