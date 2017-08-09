#include "Level.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Plugin.h"
#include "../Controller.h"
#include "../User.h"
#include "../Utils.h"

#define DEVICE_LEVEL_DEFAULT_HISTORY_RETENTION 7 // days
#define DEVICE_LEVEL_DEFAULT_TRENDS_RETENTION 36 // months

namespace micasa {

	extern std::unique_ptr<Database> g_database;
	extern std::unique_ptr<Controller> g_controller;

	using namespace std::chrono;
	using namespace nlohmann;

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
		{ Level::Unit::CELSIUS, "°C" },
		{ Level::Unit::FAHRENHEIT, "°F" },
		{ Level::Unit::PASCAL, "Pa" },
		{ Level::Unit::LUX, "lx" },
		{ Level::Unit::SECONDS, " sec" },
	};

	const std::map<Level::Unit, std::string> Level::UnitFormat = {
		{ Level::Unit::GENERIC, "%.3f" },
		{ Level::Unit::PERCENT, "%.1f" },
		{ Level::Unit::WATT, "%.0f" },
		{ Level::Unit::VOLT, "%.0f" },
		{ Level::Unit::AMPERES, "%.0f" },
		{ Level::Unit::CELSIUS, "%.1f" },
		{ Level::Unit::FAHRENHEIT, "%.1f" },
		{ Level::Unit::PASCAL, "%.0f" },
		{ Level::Unit::LUX, "%.0f" },
		{ Level::Unit::SECONDS, "%.0f" },
	};

	Level::Level( std::weak_ptr<Plugin> plugin_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) :
		Device( plugin_, id_, reference_, label_, enabled_ ),
		m_value( 0 ),
		m_updated( system_clock::now() ),
		m_rateLimiter( { 0, 0, Device::resolveUpdateSource( 0 ) } )
	{
		try {
			json result = g_database->getQueryRow<json>(
				"SELECT `value`, CAST( strftime( '%%s', 'now' ) AS INTEGER ) - CAST( strftime( '%%s', `date` ) AS INTEGER ) AS `age` "
				"FROM `device_level_history` "
				"WHERE `device_id` = %d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
			this->m_value = this->m_rateLimiter.value = jsonGet<double>( result, "value" );
			this->m_updated = system_clock::now() - seconds( jsonGet<unsigned int>( result, "age" ) );
		} catch( const Database::NoResultsException& ex_ ) {
			Logger::log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}
	};

	void Level::start() {
#ifdef _DEBUG
		assert( this->m_enabled && "Device needs to be enabled while being started." );
#endif // _DEBUG
		this->m_scheduler.schedule( randomNumber( 0, SCHEDULER_INTERVAL_5MIN ), SCHEDULER_INTERVAL_5MIN, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			this->_processTrends();
		} );
		this->m_scheduler.schedule( randomNumber( 0, SCHEDULER_INTERVAL_1HOUR ), SCHEDULER_INTERVAL_1HOUR, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			this->_purgeHistoryAndTrends();
		} );
	};

	void Level::stop() {
#ifdef _DEBUG
		assert( this->m_enabled && "Device needs to be enabled while being stopped." );
#endif // _DEBUG
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
	};

	void Level::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		if (
			! this->m_enabled
			&& ( source_ & Device::UpdateSource::PLUGIN ) != Device::UpdateSource::PLUGIN
		) {
			return;
		}

		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return;
		}

		if (
			this->getSettings()->get<bool>( "ignore_duplicates", false )
			&& this->m_value == value_
			&& this->getPlugin()->getState() >= Plugin::State::READY
		) {
			Logger::log( Logger::LogLevel::VERBOSE, this, "Ignoring duplicate value." );
			return;
		}

		double divider = this->m_settings->get<double>( "divider", 1 );
		double offset = this->m_settings->get<double>( "offset", 0 );
		if (
			(
				this->m_settings->contains( "minimum" )
				&& ( ( value_ + offset ) / divider ) < this->m_settings->get<double>( "minimum" )
			)
			|| (
				this->m_settings->contains( "maximum" )
				&& ( ( value_ + offset ) / divider ) > this->m_settings->get<double>( "maximum" )
			)
		) {
			Logger::log( this->m_enabled ? Logger::LogLevel::ERROR : Logger::LogLevel::VERBOSE, this, "Invalid value." );
			return;
		}

		if (
			this->m_settings->contains( "rate_limit" )
			&& this->getPlugin()->getState() >= Plugin::State::READY
		) {
			unsigned long rateLimit = 1000 * this->m_settings->get<double>( "rate_limit" );
			system_clock::time_point now = system_clock::now();
			system_clock::time_point next = this->m_updated + milliseconds( rateLimit );
			if ( next > now ) {
				this->m_rateLimiter.source = source_;
				if ( this->m_rateLimiter.count == 0 ) {
					this->m_rateLimiter.value = value_;
				} else {
					this->m_rateLimiter.value += value_;
				}
				this->m_rateLimiter.count++;
				auto task = this->m_rateLimiter.task.lock();
				if ( ! task ) {
					this->m_rateLimiter.task = this->m_scheduler.schedule( next, 0, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
						this->_processValue( this->m_rateLimiter.source, this->m_rateLimiter.value / this->m_rateLimiter.count );
						this->m_rateLimiter.count = 0;
					} );
				}
			} else {
				this->_processValue( source_, value_ );
			}
		} else {
			this->_processValue( source_, value_ );
		}
	};

	json Level::getJson() const {
		json result = Device::getJson();

		std::string unit = this->m_settings->get( "unit", this->m_settings->get( DEVICE_SETTING_DEFAULT_UNIT, "" ) );
		double divider = this->m_settings->get<double>( "divider", 1 );
		double offset = this->m_settings->get<double>( "offset", 0 );

		result["value"] = std::stod( stringFormat( Level::UnitFormat.at( Level::resolveTextUnit( unit ) ), ( this->m_value / divider ) + offset ) );
		result["raw_value"] = this->m_value;
		result["source"] = Device::resolveUpdateSource( this->m_source );
		result["age"] = duration_cast<seconds>( system_clock::now() - this->m_updated ).count();
		result["type"] = "level";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "generic" ) );
		result["unit"] = unit;
		result["history_retention"] = this->m_settings->get<int>( "history_retention", DEVICE_LEVEL_DEFAULT_HISTORY_RETENTION );
		result["trends_retention"] = this->m_settings->get<int>( "trends_retention", DEVICE_LEVEL_DEFAULT_TRENDS_RETENTION );
		if ( this->m_settings->contains( "minimum" ) ) {
			result["minimum"] = this->m_settings->get<double>( "minimum" );
		}
		if ( this->m_settings->contains( "maximum" ) ) {
			result["maximum"] = this->m_settings->get<double>( "maximum" );
		}
		if ( this->m_settings->contains( "divider" ) ) {
			result["divider"] = this->m_settings->get<double>( "divider" );
		}
		if ( this->m_settings->contains( "offset" ) ) {
			result["offset"] = this->m_settings->get<double>( "offset" );
		}
		if ( this->m_settings->contains( "rate_limit" ) ) {
			result["rate_limit"] = this->m_settings->get<double>( "rate_limit" );
		}

		return result;
	};

	json Level::getSettingsJson() const {
		json result = Device::getSettingsJson();
		if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, false ) ) {
			json setting = {
				{ "name", "subtype" },
				{ "label", "SubType" },
				{ "type", "list" },
				{ "options", json::array() },
				{ "class", this->m_settings->contains( "subtype" ) ? "advanced" : "normal" },
				{ "sort", 10 }
			};
			for ( auto subTypeIt = Level::SubTypeText.begin(); subTypeIt != Level::SubTypeText.end(); subTypeIt++ ) {
				setting["options"] += {
					{ "value", subTypeIt->second },
					{ "label", subTypeIt->second }
				};
			}
			result += setting;
		}
		if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_UNIT_CHANGE, false ) ) {
			json setting = {
				{ "name", "unit" },
				{ "label", "Unit" },
				{ "type", "list" },
				{ "options", json::array() },
				{ "class", this->m_settings->contains( "unit" ) ? "advanced" : "normal" },
				{ "sort", 11 }
			};
			for ( auto unitIt = Level::UnitText.begin(); unitIt != Level::UnitText.end(); unitIt++ ) {
				setting["options"] += {
					{ "value", unitIt->second },
					{ "label", unitIt->second }
				};
			}
			result += setting;
		}

		result += {
			{ "name", "history_retention" },
			{ "label", "History Retention" },
			{ "description", "How long to keep history in the database in days. Level devices store their value in the history database every 5 minutes." },
			{ "type", "int" },
			{ "minimum", 1 },
			{ "default", DEVICE_LEVEL_DEFAULT_HISTORY_RETENTION },
			{ "class", "advanced" },
			{ "sort", 12 }
		};

		result += {
			{ "name", "trends_retention" },
			{ "label", "Trends Retention" },
			{ "description", "How long to keep trends in the database in months. Level device trends keep averaged data on hourly basis and are less resource hungry." },
			{ "type", "int" },
			{ "minimum", 1 },
			{ "default", DEVICE_LEVEL_DEFAULT_TRENDS_RETENTION },
			{ "class", "advanced" },
			{ "sort", 13 }
		};

		result += {
			{ "name", "minimum" },
			{ "label", "Minimum" },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 995 }
		};

		result += {
			{ "name", "maximum" },
			{ "label", "Maximum" },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 996 }
		};

		result += {
			{ "name", "divider" },
			{ "label", "Divider" },
			{ "description", "A divider to convert the value to the designated unit." },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 997 }
		};

		result += {
			{ "name", "offset" },
			{ "label", "Offset" },
			{ "description", "A positive or negative value that is added to- or subtracted from the value in it's designated unit (after the optional divider has been applied)." },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 998 }
		};

		result += {
			{ "name", "rate_limit" },
			{ "label", "Rate Limiter" },
			{ "description", "Limits the number of updates to once per configured time period in seconds." },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 999 }
		};

		return result;
	};

	json Level::getData( unsigned int range_, const std::string& interval_, const std::string& group_ ) const {
		std::vector<std::string> validIntervals = { "hour", "day", "week", "month", "year" };
		if ( std::find( validIntervals.begin(), validIntervals.end(), interval_ ) == validIntervals.end() ) {
			return json::array();
		}
		std::string interval = interval_;
		if ( interval == "week" ) {
			interval = "day";
			range_ *= 7;
		}

		std::vector<std::string> validGroups = { "5min", "hour", "day", "month", "year" };
		if ( std::find( validGroups.begin(), validGroups.end(), group_ ) == validGroups.end() ) {
			return json::array();
		}

		std::string unit = this->m_settings->get( "unit", this->m_settings->get( DEVICE_SETTING_DEFAULT_UNIT, "" ) );
		std::string format = Level::UnitFormat.at( Level::resolveTextUnit( unit ) );
		double divider = this->m_settings->get<double>( "divider", 1 );
		double offset = this->m_settings->get<double>( "offset", 0 );

		if ( group_ == "5min" ) {
			std::string dateFormat = "%Y-%m-%d %H:%M:00";
			return g_database->getQuery<json>(
				"SELECT CAST( printf( %Q, ( `value` / %.6f ) + %.6f ) AS REAL ) AS `value`, CAST( strftime( '%%s', `date` ) AS INTEGER ) AS `timestamp`, strftime( %Q, `date` ) AS `date` "
				"FROM `device_level_history` "
				"WHERE `device_id` = %d "
				"AND `date` >= datetime( 'now', '-%d %s' ) "
				"ORDER BY `date` ASC ",
				format.c_str(),
				divider,
				offset,
				dateFormat.c_str(),
				this->m_id,
				range_,
				interval.c_str()
			);
		} else {
			std::string dateFormat = "%Y-%m-%d %H:30:00";
			std::string groupFormat = "%Y-%m-%d-%H";
			std::string start = "'start of day','+' || strftime( '%H' ) || ' hours'";
			if ( group_ == "day" ) {
				dateFormat = "%Y-%m-%d 12:00:00";
				groupFormat = "%Y-%m-%d";
				start = "'start of day'";
			} else if ( group_ == "month" ) {
				dateFormat = "%Y-%m-15 12:00:00";
				groupFormat = "%Y-%m";
				start = "'start of month'";
			} else if ( group_ == "year" ) {
				dateFormat = "%Y-06-15 12:00:00";
				groupFormat = "%Y";
				start = "'start of year'";
			}
			return g_database->getQuery<json>(
				"SELECT "
					"CAST( printf( %Q, ( avg( `average` ) / %.6f ) +  %.6f ) AS REAL ) AS `value`, "
					"CAST( printf( %Q, ( max( `max` ) / %.6f ) +  %.6f ) AS REAL ) AS `maximum`, "
					"CAST( printf( %Q, ( min( `min` ) / %.6f ) +  %.6f ) AS REAL ) AS `minimum`, "
					"CAST( strftime( '%%s', strftime( %Q, MAX( `date` ) ) ) AS INTEGER ) AS `timestamp`, "
					"strftime( %Q, MAX( `date` ) ) AS `date` "
				"FROM `device_level_trends` "
				"WHERE `device_id` = %d "
				"AND `date` >= datetime( 'now', '-%d %s', %s ) "
				"GROUP BY strftime( %Q, `date` ) "
				"ORDER BY `date` ASC ",
				format.c_str(),
				divider,
				offset,
				format.c_str(),
				divider,
				offset,
				format.c_str(),
				divider,
				offset,
				dateFormat.c_str(),
				dateFormat.c_str(),
				this->m_id,
				range_,
				interval.c_str(),
				start.c_str(),
				groupFormat.c_str()
			);
		}
	};

	void Level::_processValue( const Device::UpdateSource& source_, const t_value& value_ ) {

		// Make a local backup of the original value (the plugin might want to revert it).
		t_value previous = this->m_value;
		this->m_value = value_;

		// If the update originates from the plugin it is not send back to the plugin again.
		bool success = true;
		bool apply = true;
		if ( ( source_ & Device::UpdateSource::PLUGIN ) != Device::UpdateSource::PLUGIN ) {
			success = this->getPlugin()->updateDevice( source_, this->shared_from_this(), apply );
		}

		if ( success && apply ) {
			if ( this->m_enabled ) {
				std::string date = "strftime( '%Y-%m-%d %H:', datetime( 'now' ) ) || CASE WHEN CAST( strftime( '%M',  datetime( 'now' ) ) AS INTEGER ) < 10 THEN '0' ELSE '' END || CAST( CAST( strftime( '%M', datetime( 'now' ) ) AS INTEGER ) / 5 * 5 AS TEXT ) || ':00'";
				g_database->putQuery(
					"REPLACE INTO `device_level_history` ( `device_id`, `date`, `value`, `samples` ) "
					"VALUES ( "
						"%d, "
						"%s, "
						"COALESCE( ( "
							"SELECT printf( '%%.6f', ( ( `value` * `samples` ) + %.6f ) / ( `samples` + 1 ) ) "
							"FROM `device_level_history` "
							"WHERE `device_id` = %d "
							"AND `date` = %s "
						"), %.6f ), "
						"COALESCE( ( "
							"SELECT `samples` + 1 "
							"FROM `device_level_history` "
							"WHERE `device_id` = %d "
							"AND `date` = %s "
						"), 1 ) "
					")",
					this->m_id,
					date.c_str(),
					this->m_value,
					this->m_id,
					date.c_str(),
					this->m_value,
					this->m_id,
					date.c_str()
				);
			}
			this->m_source = source_;
			this->m_updated = system_clock::now();
			if (
				this->m_enabled
				&& this->getPlugin()->getState() >= Plugin::State::READY
			) {
				g_controller->newEvent<Level>( std::static_pointer_cast<Level>( this->shared_from_this() ), source_ );
			}
			Logger::logr( Logger::LogLevel::NORMAL, this, "New value %.3lf.", this->m_value );
		} else {
			this->m_value = previous;
		}
	};

	void Level::_processTrends() const {
		std::string hourFormat = "%Y-%m-%d %H:30:00";
		std::string groupFormat = "%Y-%m-%d-%H";

		auto trends = g_database->getQuery(
			"SELECT MAX( `value` ) AS `max`, MIN( `value` ) AS `min`, AVG( `value` ) AS `average`, strftime( %Q, MAX( `date` ) ) AS `date` "
			"FROM `device_level_history` "
			"WHERE `device_id` = %d AND `Date` > datetime( 'now','-5 hour' ) "
			"GROUP BY strftime( %Q, `date` )",
			hourFormat.c_str(),
			this->m_id,
			groupFormat.c_str()
		);
		// NOTE the first group is skipped because the select query starts somewhere in the middle of the interval and
		// the diff value is therefore incorrect.
		for ( auto trendsIt = trends.begin(); trendsIt != trends.end(); trendsIt++ ) {
			if ( trendsIt != trends.begin() ) {
				g_database->putQuery(
					"REPLACE INTO `device_level_trends` ( `device_id`, `min`, `max`, `average`, `date` ) "
					"VALUES ( %d, %.6f, %.6f, %.6f, %Q )",
					this->m_id,
					std::stod( (*trendsIt)["min"] ),
					std::stod( (*trendsIt)["max"] ),
					std::stod( (*trendsIt)["average"] ),
					(*trendsIt)["date"].c_str()
				);
			}
		}
	};

	void Level::_purgeHistoryAndTrends() const {
		g_database->putQuery(
			"DELETE FROM `device_level_history` "
			"WHERE `device_id` = %d "
			"AND `date` < datetime( 'now','-%d day' )",
			this->m_id,
			this->m_settings->get<int>( "history_retention", DEVICE_LEVEL_DEFAULT_HISTORY_RETENTION )
		);
		g_database->putQuery(
			"DELETE FROM `device_level_trends` "
			"WHERE `device_id` = %d "
			"AND `date` < datetime( 'now','-%d month' )",
			this->m_id,
			this->m_settings->get<int>( "trends_retention", DEVICE_LEVEL_DEFAULT_TRENDS_RETENTION )
		);
	};

}; // namespace micasa
