#include "Text.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;

	using namespace std::chrono;
	using namespace nlohmann;

	const Device::Type Text::type = Device::Type::TEXT;

	const std::map<Text::SubType, std::string> Text::SubTypeText = {
		{ Text::SubType::GENERIC, "generic" },
		{ Text::SubType::WIND_DIRECTION, "wind_direction" },
		{ Text::SubType::NOTIFICATION, "notification" }
	};

	Text::Text( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) : Device( hardware_, id_, reference_, label_, enabled_ ) {
		try {
			this->m_value = g_database->getQueryValue<std::string>(
				"SELECT `value` "
				"FROM `device_text_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
		} catch( const Database::NoResultsException& ex_ ) {
			g_logger->log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}

		// The device data crunching tasks are scheduled here. To prevent all devices from crunching data at the same
		// time, a start offset is used.
		static volatile unsigned int offset = 0;
		offset += ( 1000 * 15 ); // 15 seconds interval
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, NULL, [this]( Scheduler::Task<>& ) -> void {
			this->_purgeHistory();
		} )->advance( offset % SCHEDULER_INTERVAL_HOUR );
	};

	void Text::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
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
					this->m_rateLimiter.task = this->m_scheduler.schedule( next, 0, 1, NULL, [this]( Scheduler::Task<>& task_ ) -> void {
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

	json Text::getJson( bool full_ ) const {
		json result = Device::getJson( full_ );
		result["value"] = this->getValue();
		result["type"] = "text";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "" ) );
		if ( this->m_settings->contains( "rate_limit" ) ) {
			result["rate_limit"] = this->m_settings->get<double>( "rate_limit" );
		}
		
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		
		return result;
	};

	json Text::getSettingsJson() const {
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
			for ( auto subTypeIt = Text::SubTypeText.begin(); subTypeIt != Text::SubTypeText.end(); subTypeIt++ ) {
				setting["options"] += {
					{ "value", subTypeIt->second },
					{ "label", subTypeIt->second }
				};
			}
			result += setting;
		}

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
	
	json Text::getData( unsigned int range_, const std::string& interval_ ) const {
		std::vector<std::string> validIntervals = { "day", "week", "month", "year" };
		if ( std::find( validIntervals.begin(), validIntervals.end(), interval_ ) == validIntervals.end() ) {
			return json::array();
		}
		std::string interval = interval_;
		if ( interval == "week" ) {
			interval = "day";
			range_ *= 7;
		}

		return g_database->getQuery<json>(
			"SELECT `value`, CAST(strftime('%%s',`date`) AS INTEGER) AS `timestamp` "
			"FROM `device_text_history` "
			"WHERE `device_id`=%d "
			"AND `date` >= datetime('now','-%d %s') "
			"ORDER BY `date` ASC ",
			this->m_id,
			range_,
			interval.c_str()
		);
	};
	
	void Text::_processValue( const Device::UpdateSource& source_, const t_value& value_ ) {

		// Make a local backup of the original value (the hardware might want to revert it).
		t_value previous = this->m_value;
		this->m_value = value_;
		
		// If the update originates from the hardware, or the value is NOT different than the current value,
		// do not send it to the hardware again.
		bool success = true;
		bool apply = true;
		if ( ( source_ & Device::UpdateSource::HARDWARE ) != Device::UpdateSource::HARDWARE ) {
			success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		}
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_text_history` (`device_id`, `value`) "
				"VALUES (%d, %Q)",
				this->m_id,
				this->m_value.c_str()
			);
			this->m_previousValue = previous; // before newEvent so previous value can be provided
			if ( this->isEnabled() ) {
				g_controller->newEvent<Text>( std::static_pointer_cast<Text>( this->shared_from_this() ), source_ );
			}
			g_logger->logr( this->isEnabled() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, this, "New value %s.", this->m_value.c_str() );
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

	void Text::_purgeHistory() const {
		g_database->putQuery(
			"DELETE FROM `device_text_history` "
			"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
			, this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
		);
	};
	
}; // namespace micasa
