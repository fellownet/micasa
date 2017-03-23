#include <atomic>
#include <algorithm>

#include "Counter.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"
#include "../User.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;

	using namespace std::chrono;
	using namespace nlohmann;

	const Device::Type Counter::type = Device::Type::COUNTER;

	const std::map<Counter::SubType, std::string> Counter::SubTypeText = {
		{ Counter::SubType::GENERIC, "generic" },
		{ Counter::SubType::ENERGY, "energy" },
		{ Counter::SubType::GAS, "gas" },
		{ Counter::SubType::WATER, "water" },
	};

	const std::map<Counter::Unit, std::string> Counter::UnitText = {
		{ Counter::Unit::GENERIC, "" },
		{ Counter::Unit::KILOWATTHOUR, "kWh" },
		{ Counter::Unit::M3, "M3" }
	};

	Counter::Counter( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) : Device( hardware_, id_, reference_, label_, enabled_ ) {
		try {
			this->m_value = this->m_rateLimiter.value = g_database->getQueryValue<Counter::t_value>(
				"SELECT `value` "
				"FROM `device_counter_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
		} catch( const Database::NoResultsException& ex_ ) {
			// A counter device always needs a starting point.
			this->m_value = this->m_rateLimiter.value = 0;
			g_logger->log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}

		// To avoid all devices from crunching data at the same time, the tasks are started with a small time offset.
		static std::atomic<unsigned int> offset( 0 );
		offset += ( 1000 * 11 ); // 11 seconds interval
		this->m_scheduler.schedule( system_clock::now() + milliseconds( SCHEDULER_INTERVAL_5MIN + ( offset % SCHEDULER_INTERVAL_5MIN ) ), SCHEDULER_INTERVAL_5MIN, SCHEDULER_INFINITE, NULL, [this]( Scheduler::Task<>& ) {
			this->_processTrends();
		} );
		this->m_scheduler.schedule( system_clock::now() + milliseconds( SCHEDULER_INTERVAL_HOUR + ( offset % SCHEDULER_INTERVAL_HOUR ) ), SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, NULL, [this]( Scheduler::Task<>& ) {
			this->_purgeHistory();
		} );
	};

	void Counter::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			auto configured = Device::resolveUpdateSource( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) );
			auto requested = Device::resolveUpdateSource( source_ );
			g_logger->logr( Logger::LogLevel::ERROR, this, "Invalid update source (%d vs %d).", configured, requested );
			return;
		}
		
		if (
			this->getSettings()->get<bool>( "ignore_duplicates", this->getType() == Device::Type::SWITCH || this->getType() == Device::Type::TEXT )
			&& this->m_value == value_
			&& static_cast<unsigned short>( source_ & Device::UpdateSource::INIT ) == 0
		) {
			g_logger->log( Logger::LogLevel::VERBOSE, this, "Ignoring duplicate value." );
			return;
		}

		if ( this->m_settings->contains( "rate_limit" ) ) {
			unsigned long rateLimit = 1000 * this->m_settings->get<double>( "rate_limit" );
			system_clock::time_point now = system_clock::now();
			system_clock::time_point next = this->m_rateLimiter.last + milliseconds( rateLimit );
			if ( next > now ) {
				this->m_rateLimiter.source = source_;
				this->m_rateLimiter.value = value_;
				auto task = this->m_rateLimiter.task.lock();
				if ( ! task ) {
					this->m_rateLimiter.task = this->m_scheduler.schedule( next, 0, 1, NULL, [this]( Scheduler::Task<>& task_ ) {
						this->_processValue( this->m_rateLimiter.source, this->m_rateLimiter.value );
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

	void Counter::incrementValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		this->updateValue( source_, std::max( this->m_value, this->m_rateLimiter.value ) + value_ );
	};
	
	json Counter::getJson( bool full_ ) const {
		double divider = this->m_settings->get<double>( "divider", 1 );

		json result = Device::getJson( full_ );
		result["value"] = round( ( this->m_value / divider ) * 1000.0f ) / 1000.0f;
		result["raw_value"] = this->m_value;
		result["type"] = "counter";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "" ) );
		result["unit"] = this->m_settings->get( "unit", this->m_settings->get( DEVICE_SETTING_DEFAULT_UNIT, "" ) );
		if ( this->m_settings->contains( "divider" ) ) {
			result["divider"] = this->m_settings->get<double>( "divider" );
		}
		if ( this->m_settings->contains( "rate_limit" ) ) {
			result["rate_limit"] = this->m_settings->get<double>( "rate_limit" );
		}
	
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		
		return result;
	};

	json Counter::getSettingsJson() const {
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
			for ( auto subTypeIt = Counter::SubTypeText.begin(); subTypeIt != Counter::SubTypeText.end(); subTypeIt++ ) {
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
			for ( auto unitIt = Counter::UnitText.begin(); unitIt != Counter::UnitText.end(); unitIt++ ) {
				setting["options"] += {
					{ "value", unitIt->second },
					{ "label", unitIt->second }
				};
			}
			result += setting;
		}

		result += {
			{ "name", "divider" },
			{ "label", "Divider" },
			{ "description", "A divider to convert the value to the designated unit." },
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

	json Counter::getData( unsigned int range_, const std::string& interval_, const std::string& group_ ) const {
		std::vector<std::string> validIntervals = { "day", "week", "month", "year" };
		if ( std::find( validIntervals.begin(), validIntervals.end(), interval_ ) == validIntervals.end() ) {
			return json::array();
		}
		std::string interval = interval_;
		if ( interval == "week" ) {
			interval = "day";
			range_ *= 7;
		}

		std::vector<std::string> validGroups = { /*"none",*/ "hour", "day", "month", "year" };
		if ( std::find( validGroups.begin(), validGroups.end(), group_ ) == validGroups.end() ) {
			return json::array();
		}

		double divider = this->m_settings->get<double>( "divider", 1 );

		/*
		if ( group_ == "none" ) {
			return g_database->getQuery<json>(
				"SELECT printf(\"%%.3f\", ( `value` + %.6f ) / %.6f ) AS `value`, CAST( strftime('%%s',`date`) AS INTEGER ) AS `timestamp`, `date` "
				"FROM `device_counter_history` "
				"WHERE `device_id`=%d "
				"AND `date` >= datetime('now','-%d %s') "
				"ORDER BY `date` ASC ",
				offset,
				divider,
				this->m_id,
				range_,
				interval.c_str()
			);
		} else {
		*/

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
			"SELECT printf(\"%%.3f\", sum(`diff`) / %.6f ) AS `value`, CAST( strftime( '%%s', strftime( %Q, MAX(`date`) ) ) AS INTEGER ) AS `timestamp`, strftime( %Q, MAX(`date`) ) AS `date` "
			"FROM `device_counter_trends` "
			"WHERE `device_id`=%d "
			"AND `date` >= datetime('now','-%d %s') "
			"GROUP BY strftime(%Q, `date`) "
			"ORDER BY `date` ASC ",
			divider,
			dateFormat.c_str(),
			dateFormat.c_str(),
			this->m_id,
			range_,
			interval.c_str(),
			groupFormat.c_str()
		);

		//}
	};

	void Counter::_processValue( const Device::UpdateSource& source_, const t_value& value_ ) {

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
				"INSERT INTO `device_counter_history` (`device_id`, `value`) "
				"VALUES (%d, %.6lf)",
				this->m_id,
				this->m_value
			);
			this->m_previousValue = previous; // before newEvent so previous value can be provided
			if ( this->isEnabled() ) {
				g_controller->newEvent<Counter>( std::static_pointer_cast<Counter>( this->shared_from_this() ), source_ );
			}
			g_logger->logr( this->isEnabled() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, this, "New value %.3lf.", this->m_value );
		} else {
			this->m_value = previous;
		}
		if (
			success
			&& ( source_ & Device::UpdateSource::INIT ) != Device::UpdateSource::INIT
		) {
			this->touch();
		}
	};

	void Counter::_processTrends() const {
		std::string hourFormat = "%Y-%m-%d %H:30:00";
		std::string groupFormat = "%Y-%m-%d-%H";

		auto trends = g_database->getQuery(
			"SELECT MAX(`date`) AS `date1`, strftime(%Q, MAX(`date`)) AS `date2`, MAX(`value`)-MIN(`value`) AS `diff` "
			"FROM `device_counter_history` "
			"WHERE `device_id`=%d AND `Date` > datetime('now','-4 hour') "
			"GROUP BY strftime(%Q, `date`)",
			hourFormat.c_str(),
			this->m_id,
			groupFormat.c_str()
		);
		for ( auto trendsIt = trends.begin(); trendsIt != trends.end(); trendsIt++ ) {
			auto value = g_database->getQueryValue<double>(
				"SELECT `value` "
				"FROM `device_counter_history` "
				"WHERE `device_id`=%d AND `date`=%Q",
				this->m_id,
				(*trendsIt)["date1"].c_str()
			);
			g_database->putQuery(
				"REPLACE INTO `device_counter_trends` (`device_id`, `last`, `diff`, `date`) "
				"VALUES (%d, %.6lf, %.6lf, %Q)",
				this->m_id,
				value,
				std::stod( (*trendsIt)["diff"] ),
				(*trendsIt)["date2"].c_str()
			);
		}
	};

	void Counter::_purgeHistory() const {
		g_database->putQuery(
			"DELETE FROM `device_counter_history` "
			"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')",
			this->m_id,
			this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 7 )
		);
	};

}; // namespace micasa
