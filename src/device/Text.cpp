#include "Text.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Plugin.h"
#include "../Controller.h"
#include "../Utils.h"

#define DEVICE_TEXT_DEFAULT_HISTORY_RETENTION 7 // days

namespace micasa {

	extern std::unique_ptr<Database> g_database;
	extern std::unique_ptr<Controller> g_controller;

	using namespace std::chrono;
	using namespace nlohmann;

	const Device::Type Text::type = Device::Type::TEXT;

	const std::map<Text::SubType, std::string> Text::SubTypeText = {
		{ Text::SubType::GENERIC, "generic" },
		{ Text::SubType::WIND_DIRECTION, "wind_direction" },
		{ Text::SubType::NOTIFICATION, "notification" }
	};

	Text::Text( std::weak_ptr<Plugin> plugin_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) :
		Device( plugin_, id_, reference_, label_, enabled_ ),
		m_value( "" ),
		m_updated( system_clock::now() ),
		m_rateLimiter( { "", Device::resolveUpdateSource( 0 ) } ),
		m_logger( { "", 0 } )
	{
		try {
			json result = g_database->getQueryRow<json>(
				"SELECT `value`, CAST( strftime( '%%s', 'now' ) AS INTEGER ) - CAST( strftime( '%%s', `date` ) AS INTEGER ) AS `age` "
				"FROM `device_text_history` "
				"WHERE `device_id` = %d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
			this->m_value = this->m_rateLimiter.value = jsonGet<std::string>( result, "value" );
			this->m_updated = system_clock::now() - seconds( jsonGet<unsigned int>( result, "age" ) );
		} catch( const Database::NoResultsException& ex_ ) {
			Logger::log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}
	};

	void Text::start() {
#ifdef _DEBUG
		assert( this->m_enabled && "Device needs to be enabled while being started." );
#endif // _DEBUG
		this->m_scheduler.schedule( randomNumber( 0, SCHEDULER_INTERVAL_HOUR ), SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			this->_purgeHistory();
		} );

		// Fake an update of the settings to make sure the device is added to the logger and have it receive log
		// messages.
		this->putSettingsJson( {
			{ "send_log", this->m_settings->get<bool>( "send_log", false ) },
			{ "send_log_level", this->m_settings->get<int>( "send_log_level", Logger::resolveLogLevel( Logger::LogLevel::ERROR ) ) }
		} );
	};

	void Text::stop() {
#ifdef _DEBUG
		assert( this->m_enabled && "Device needs to be enabled while being stopped." );
#endif // _DEBUG
		Logger::removeReceiver( std::static_pointer_cast<Text>( this->shared_from_this() ) );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
	};

	void Text::updateValue( Device::UpdateSource source_, t_value value_ ) {
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

		if (
			this->m_settings->contains( "rate_limit" )
			&& this->getPlugin()->getState() >= Plugin::State::READY
		) {
			unsigned long rateLimit = 1000 * this->m_settings->get<double>( "rate_limit" );
			system_clock::time_point now = system_clock::now();
			system_clock::time_point next = this->m_updated + milliseconds( rateLimit );
			if ( next > now ) {
				this->m_rateLimiter.source = source_;
				this->m_rateLimiter.value = value_;
				auto task = this->m_rateLimiter.task.lock();
				if ( ! task ) {
					this->m_rateLimiter.task = this->m_scheduler.schedule( next, 0, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
						this->_processValue( this->m_rateLimiter.source, this->m_rateLimiter.value );
					} );
				}
			} else {
				this->_processValue( source_, value_ );
			}
		} else {
			this->_processValue( source_, value_ );
		}
	};

	json Text::getJson() const {
		json result = Device::getJson();

		result["value"] = this->m_value;
		result["source"] = Device::resolveUpdateSource( this->m_source );
		result["age"] = duration_cast<seconds>( system_clock::now() - this->m_updated ).count();
		result["type"] = "text";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "generic" ) );
		result["history_retention"] = this->m_settings->get<int>( "history_retention", DEVICE_TEXT_DEFAULT_HISTORY_RETENTION );
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
			{ "name", "history_retention" },
			{ "label", "History Retention" },
			{ "description", "How long to keep history in the database in days. Text devices store each collected value in the history database." },
			{ "type", "int" },
			{ "minimum", 1 },
			{ "default", DEVICE_TEXT_DEFAULT_HISTORY_RETENTION },
			{ "class", "advanced" },
			{ "sort", 12 }
		};

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
		std::vector<std::string> validIntervals = { "hour", "day", "week", "month", "year" };
		if ( std::find( validIntervals.begin(), validIntervals.end(), interval_ ) == validIntervals.end() ) {
			return json::array();
		}
		std::string interval = interval_;
		if ( interval == "week" ) {
			interval = "day";
			range_ *= 7;
		}
		return g_database->getQuery<json>(
			"SELECT `value`, CAST( strftime( '%%s', `date` ) AS INTEGER ) AS `timestamp` "
			"FROM `device_text_history` "
			"WHERE `device_id` = %d "
			"AND `date` >= datetime( 'now', '-%d %s' ) "
			"ORDER BY `date` ASC ",
			this->m_id,
			range_,
			interval.c_str()
		);
	};

	void Text::log( const Logger::LogLevel& logLevel_, const std::string& message_ ) {
		auto task = this->m_logger.task.lock();
		if ( ! task ) {
			this->m_logger.task = this->m_scheduler.schedule( SCHEDULER_INTERVAL_1MIN, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
				if ( this->m_logger.repeated > 0 ) {
					this->updateValue( UpdateSource::SYSTEM, "Last message was repeated " + std::to_string( this->m_logger.repeated ) + " times." );
				}
				this->m_logger.last = "";
				this->m_logger.repeated = 0;
			} );
		}
		if ( message_ == this->m_logger.last ) {
			this->m_logger.repeated++;
			return;
		}
		if ( task ) {
			this->m_scheduler.proceed( 0, task );
		}
		this->m_logger.last = message_;

		// The actual updating of the value with the log is done in a separate task because the action itself might
		// generate a log and would then cause a deadlock by the logger.
		this->m_scheduler.schedule( 0, 1, this, [=]( std::shared_ptr<Scheduler::Task<>> ) {
			if ( logLevel_ == Logger::LogLevel::ERROR ) {
				this->updateValue( UpdateSource::SYSTEM, "Error: " + message_ );
			} else if ( logLevel_ == Logger::LogLevel::WARNING ) {
				this->updateValue( UpdateSource::SYSTEM, "Warning: " + message_ );
			} else if ( logLevel_ == Logger::LogLevel::SCRIPT ) {
				this->updateValue( UpdateSource::SYSTEM, "Script: " + message_ );
			}
		} );
	};

	void Text::_processValue( const Device::UpdateSource& source_, const t_value& value_ ) {

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
				g_database->putQuery(
					"INSERT INTO `device_text_history` ( `device_id`, `value` ) "
					"VALUES ( %d, %Q )",
					this->m_id,
					this->m_value.c_str()
				);
			}
			this->m_source = source_;
			this->m_updated = system_clock::now();
			if (
				this->m_enabled
				&& this->getPlugin()->getState() >= Plugin::State::READY
			) {
				g_controller->newEvent<Text>( std::static_pointer_cast<Text>( this->shared_from_this() ), source_ );
			}
			Logger::logr( Logger::LogLevel::NORMAL, this, "New value %s.", this->m_value.c_str() );
		} else {
			this->m_value = previous;
		}
	};

	void Text::_purgeHistory() const {
		g_database->putQuery(
			"DELETE FROM `device_text_history` "
			"WHERE `device_id` = %d "
			"AND `date` < datetime( 'now','-%d day' )",
			this->m_id,
			this->m_settings->get<int>( "history_retention", DEVICE_TEXT_DEFAULT_HISTORY_RETENTION )
		);
	};

}; // namespace micasa
