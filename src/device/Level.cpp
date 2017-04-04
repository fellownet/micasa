#include "Level.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"
#include "../User.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;

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
	
	Level::Level( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) :
		Device( hardware_, id_, reference_, label_, enabled_ ),
		m_value( 0 ),
		m_previousValue( 0 ),
		m_rateLimiter( { 0, 0, Device::resolveUpdateSource( 0 ), system_clock::now() } )
	{
		try {
			this->m_value = g_database->getQueryValue<Level::t_value>(
				"SELECT `value` "
				"FROM `device_level_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
		} catch( const Database::NoResultsException& ex_ ) {
			Logger::log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}
	};

	void Level::_init() {
		// To avoid all devices from crunching data at the same time, the tasks are started with a small time offset.
		static std::atomic<unsigned int> offset( 0 );
		offset += ( 1000 * 11 ); // 11 seconds interval
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN + ( offset % SCHEDULER_INTERVAL_5MIN ), SCHEDULER_INTERVAL_5MIN, SCHEDULER_INFINITE, this, "level trends", [this]( Scheduler::Task<>& ) {
			this->_processTrends();
		} );
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_HOUR + ( offset % SCHEDULER_INTERVAL_HOUR ), SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, this, "level purge", [this]( Scheduler::Task<>& ) {
			this->_purgeHistory();
		} );
	};

	void Level::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return;
		}

		if (
			this->getSettings()->get<bool>( "ignore_duplicates", this->getType() == Device::Type::SWITCH || this->getType() == Device::Type::TEXT )
			&& this->m_value == value_
			&& this->getHardware()->getState() == Hardware::State::READY
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
			Logger::log( Logger::LogLevel::ERROR, this, "Invalid value." );
			return;
		}

		if (
			this->m_settings->contains( "rate_limit" )
			&& this->getHardware()->getState() == Hardware::State::READY
		) {
			unsigned long rateLimit = 1000 * this->m_settings->get<double>( "rate_limit" );
			system_clock::time_point now = system_clock::now();
			system_clock::time_point next = this->m_rateLimiter.last + milliseconds( rateLimit );
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
					this->m_rateLimiter.task = this->m_scheduler.schedule( next, 0, 1, NULL, "level ratelimiter", [this]( Scheduler::Task<>& task_ ) {
						this->_processValue( this->m_rateLimiter.source, this->m_rateLimiter.value / this->m_rateLimiter.count );
						this->m_rateLimiter.count = 0;
						this->m_rateLimiter.last = task_.time;
					} );
				}
			} else {
				this->_processValue( source_, value_ );
				this->m_rateLimiter.last = now;
			}
		} else {
			this->_processValue( source_, value_ );
		}
	};

	json Level::getJson( bool full_ ) const {
		double divider = this->m_settings->get<double>( "divider", 1 );
		double offset = this->m_settings->get<double>( "offset", 0 );

		json result = Device::getJson( full_ );
		//std::stringstream ss;
		//ss << std::fixed << std::setprecision( 3 ) << this->getValue();
		//result["value"] = ss.str();
		result["value"] = round( ( ( this->m_value / divider ) + offset ) * 1000.0f ) / 1000.0f;
		result["raw_value"] = this->m_value;
		result["type"] = "level";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "" ) );
		result["unit"] = this->m_settings->get( "unit", this->m_settings->get( DEVICE_SETTING_DEFAULT_UNIT, "" ) );

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

		try {
			json timeProperties = g_database->getQueryRow<json>(
				"SELECT CAST( strftime( '%%s', MAX( `date` ) OR 'now' ) AS INTEGER ) AS `last_update`, CAST( strftime( '%%s', 'now' ) AS INTEGER ) - CAST( strftime( '%%s', MAX( `date` ) OR 'now' ) AS INTEGER ) AS `age` "
				"FROM `device_level_history` "
				"WHERE `device_id`=%d ",
				this->getId()
			);
			result["last_update"] = timeProperties["last_update"].get<unsigned long>();
			result["age"] = timeProperties["age"].get<unsigned long>();
		} catch( json::exception ex_ ) { }

		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
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
		std::vector<std::string> validIntervals = { "day", "week", "month", "year" };
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

		double divider = this->m_settings->get<double>( "divider", 1 );
		double offset = this->m_settings->get<double>( "offset", 0 );

		if ( group_ == "5min" ) {
			std::string dateFormat = "%Y-%m-%d %H:%M:00";
			return g_database->getQuery<json>(
				"SELECT printf(\"%%.3f\", ( avg(`value`) + %.6f ) / %.6f ) AS `value`, CAST( strftime( '%%s', strftime( %Q, MIN(`date`) ) ) AS INTEGER ) AS `timestamp`, strftime( %Q, MIN( `date` ) ) AS `date` "
				"FROM `device_level_history` "
				"WHERE `device_id`=%d "
				"AND `date` >= datetime('now','-%d %s') "
				"GROUP BY printf( \"%%s-%%d\", strftime( '%%Y-%%m-%%d-%%H', `date` ), CAST( strftime( '%%M', `date` ) / 5 AS INTEGER ) ) "
				"ORDER BY `date` ASC ",
				offset,
				divider,
				dateFormat.c_str(),
				dateFormat.c_str(),
				this->m_id,
				range_,
				interval.c_str()
			);
		} else {
			std::string dateFormat = "%Y-%m-%d %H:30:00";
			std::string groupFormat = "%Y-%m-%d-%H";
			if ( group_ == "day" ) {
				dateFormat = "%Y-%m-%d 12:00:00";
				groupFormat = "%Y-%m-%d";
			} else if ( group_ == "month" ) {
				dateFormat = "%Y-%m-15 12:00:00";
				groupFormat = "%Y-%m";
			} else if ( group_ == "year" ) {
				dateFormat = "%Y-06-15 12:00:00";
				groupFormat = "%Y";
			}
			return g_database->getQuery<json>(
				"SELECT printf(\"%%.3f\", ( avg(`average`) + %.6f ) /  %.6f ) AS `value`, CAST( strftime( '%%s', strftime( %Q, MAX(`date`) ) ) AS INTEGER ) AS `timestamp`, strftime( %Q, MAX( `date` ) ) AS `date` "
				"FROM `device_level_trends` "
				"WHERE `device_id`=%d "
				"AND `date` >= datetime('now','-%d %s') "
				"GROUP BY strftime( %Q, `date` ) "
				"ORDER BY `date` ASC ",
				offset,
				divider,
				dateFormat.c_str(),
				dateFormat.c_str(),
				this->m_id,
				range_,
				interval.c_str(),
				groupFormat.c_str()
			);
		}
	};

	void Level::_processValue( const Device::UpdateSource& source_, const t_value& value_ ) {

		// Make a local backup of the original value (the hardware might want to revert it).
		t_value previous = this->m_value;
		this->m_value = value_;
			
		// If the update originates from the hardware, do not send it to the hardware again.
		bool success = true;
		bool apply = true;
		if ( ( source_ & Device::UpdateSource::HARDWARE ) != Device::UpdateSource::HARDWARE ) {
			success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		}

		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_level_history` (`device_id`, `value`) "
				"VALUES (%d, %.6lf)",
				this->m_id,
				this->m_value
			);

			this->m_previousValue = previous; // before newEvent so previous value can be provided
			if (
				this->isEnabled()
				&& this->getHardware()->getState() != Hardware::State::READY
			) {
				g_controller->newEvent<Level>( std::static_pointer_cast<Level>( this->shared_from_this() ), source_ );
			}
			Logger::logr( this->isEnabled() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, this, "New value %.3lf.", this->m_value );
		} else {
			this->m_value = previous;
		}
	};

	void Level::_processTrends() const {
		std::string hourFormat = "%Y-%m-%d %H:30:00";
		std::string groupFormat = "%Y-%m-%d-%H";

		auto trends = g_database->getQuery(
			"SELECT MAX(`value`) AS `max`, MIN(`value`) AS `min`, AVG(`value`) AS `average`, strftime(%Q, MAX(`date`)) AS `date` "
			"FROM `device_level_history` "
			"WHERE `device_id`=%d AND `Date` > datetime('now','-5 hour') "
			"GROUP BY strftime(%Q, `date`)",
			hourFormat.c_str(),
			this->m_id,
			groupFormat.c_str()
		);
		// NOTE the first group is skipped because the select query starts somewhere in the middle of the interval and
		// the diff value is therefore incorrect.
		for ( auto trendsIt = trends.begin(); trendsIt != trends.end(); trendsIt++ ) {
			if ( trendsIt != trends.begin() ) {
				g_database->putQuery(
					"REPLACE INTO `device_level_trends` (`device_id`, `min`, `max`, `average`, `date`) "
					"VALUES (%d, %.6lf, %.6lf, %.6lf, %Q)",
					this->m_id,
					std::stod( (*trendsIt)["min"] ),
					std::stod( (*trendsIt)["max"] ),
					std::stod( (*trendsIt)["average"] ),
					(*trendsIt)["date"].c_str()
				);
			}
		}
	};

	void Level::_purgeHistory() const {
		g_database->putQuery(
			"DELETE FROM `device_level_history` "
			"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')",
			this->m_id,
			this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 7 )
		);
	};

}; // namespace micasa
