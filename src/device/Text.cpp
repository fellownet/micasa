#include "Text.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;

	using namespace std::chrono;
	using namespace nlohmann;

	const Device::Type Text::type = Device::Type::TEXT;

	const std::map<Text::SubType, std::string> Text::SubTypeText = {
		{ Text::SubType::GENERIC, "generic" },
		{ Text::SubType::WIND_DIRECTION, "wind_direction" },
		{ Text::SubType::NOTIFICATION, "notification" }
	};

	Text::Text( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) :
		Device( hardware_, id_, reference_, label_, enabled_ ),
		m_value( "" ),
		m_previousValue( "" ),
		m_rateLimiter( { "", Device::resolveUpdateSource( 0 ), system_clock::now() } )
	{
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
			Logger::log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}
	};

	void Text::_init() {
		// To avoid all devices from crunching data at the same time, the tasks are started with a small time offset.
		static std::atomic<unsigned int> offset( 0 );
		offset += ( 1000 * 11 ); // 11 seconds interval
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_HOUR + ( offset % SCHEDULER_INTERVAL_HOUR ), SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, this, "text purge", [this]( Scheduler::Task<>& ) {
			this->_purgeHistory();
		} );

		// Fake an update of the settings to make sure the device is added to the logger and have it receive log
		// messages.
		this->putSettingsJson( {
			{ "send_log", this->m_settings->get<bool>( "send_log", false ) },
			{ "send_log_level", this->m_settings->get<int>( "send_log_level", Logger::resolveLogLevel( Logger::LogLevel::ERROR ) ) }
		} );
	};

	void Text::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
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
		
		if (
			this->m_settings->contains( "rate_limit" )
			&& this->getHardware()->getState() == Hardware::State::READY
		) {
			unsigned long rateLimit = 1000 * this->m_settings->get<double>( "rate_limit" );
			system_clock::time_point now = system_clock::now();
			system_clock::time_point next = this->m_rateLimiter.last + milliseconds( rateLimit );
			if ( next > now ) {
				this->m_rateLimiter.source = source_;
				this->m_rateLimiter.value = value_;
				auto task = this->m_rateLimiter.task.lock();
				if ( ! task ) {
					this->m_rateLimiter.task = this->m_scheduler.schedule( next, 0, 1, NULL, "text ratelimiter", [this]( Scheduler::Task<>& task_ ) {
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
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & UpdateSource::SYSTEM ) == UpdateSource::SYSTEM ) {
			bool sendLog = this->m_settings->get<bool>( "send_log", false );
			result["send_log"] = sendLog;
			if ( sendLog ) {
				result["send_log_level"] = this->m_settings->get<int>( "send_log_level", Logger::resolveLogLevel( Logger::LogLevel::ERROR ) );
			}
		}

		try {
			json timeProperties = g_database->getQueryRow<json>(
				"SELECT CAST( strftime( '%%s', MAX( `date` ) ) AS INTEGER ) AS `last_update`, CAST( strftime( '%%s', 'now' ) AS INTEGER ) - CAST( strftime( '%%s', MAX( `date` ) ) AS INTEGER ) AS `age` "
				"FROM `device_text_history` "
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

	json Text::getSettingsJson() const {
		json result = Device::getSettingsJson();
		if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, false ) ) {
			json setting = {
				{ "name", "subtype" },
				{ "label", "SubType" },
				{ "type", "list" },
				{ "options", json::array() },
				{ "class", "advanced" },
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
			{ "sort", 998 }
		};
		
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & UpdateSource::SYSTEM ) == UpdateSource::SYSTEM ) {
			result += {
				{ "name", "send_log" },
				{ "label", "Send Log" },
				{ "description", "If enabled, system warnings and errors will be send to this text device." },
				{ "type", "boolean" },
				{ "class", "advanced" },
				{ "sort", 999 },
				{ "settings", {
					{
						{ "name", "send_log_level" },
						{ "label", "Log Level" },
						{ "type", "list" },
						{ "mandatory", true },
						{ "default", Logger::resolveLogLevel( Logger::LogLevel::ERROR ) },
						{ "options", {
							{
								{ "value", Logger::resolveLogLevel( Logger::LogLevel::ERROR ) },
								{ "label", "Errors" },
							},
							{
								{ "value", Logger::resolveLogLevel( Logger::LogLevel::WARNING ) },
								{ "label", "Errors and warnings" },
							},
							{
								{ "value", Logger::resolveLogLevel( Logger::LogLevel::SCRIPT ) },
								{ "label", "Errors, warnings and scripts" },
							}
						} }
					}
				} }
			};
		}

		return result;
	};
	
	void Text::putSettingsJson( const nlohmann::json& settings_ ) {
		Logger::removeReceiver( std::static_pointer_cast<Text>( this->shared_from_this() ) );
		if ( jsonGet<bool>( settings_, "send_log" ) ) {
			Logger::addReceiver(
				std::static_pointer_cast<Text>( this->shared_from_this() ),
				Logger::resolveLogLevel( jsonGet<int>( settings_, "send_log_level" ) )
			);
		}
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
	
	void Text::log( const Logger::LogLevel& logLevel_, const std::string& message_ ) {
		std::string text = message_;
		if ( logLevel_ == Logger::LogLevel::ERROR ) {
			text = "Error: " + text;
		} else if ( logLevel_ == Logger::LogLevel::WARNING ) {
			text = "Warning: " + text;
		} else if ( logLevel_ == Logger::LogLevel::SCRIPT ) {
			text = "Script: " + text;
		}
		this->updateValue( UpdateSource::SYSTEM, text );
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
			if (
				this->isEnabled()
				&& this->getHardware()->getState() != Hardware::State::READY
			) {
				g_controller->newEvent<Text>( std::static_pointer_cast<Text>( this->shared_from_this() ), source_ );
			}
			Logger::logr( this->isEnabled() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, this, "New value %s.", this->m_value.c_str() );
		} else {
			this->m_value = previous;
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
