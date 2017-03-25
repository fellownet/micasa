#include "Switch.h"

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

	const Device::Type Switch::type = Device::Type::SWITCH;

	const std::map<Switch::SubType, std::string> Switch::SubTypeText = {
		{ Switch::SubType::GENERIC, "generic" },
		{ Switch::SubType::LIGHT, "light" },
		{ Switch::SubType::DOOR_CONTACT, "door_contact" },
		{ Switch::SubType::BLINDS, "blinds" },
		{ Switch::SubType::MOTION_DETECTOR, "motion_detector" },
		{ Switch::SubType::FAN, "fan" },
		{ Switch::SubType::HEATER, "heater" },
		{ Switch::SubType::BELL, "bell" },
		{ Switch::SubType::ACTION, "action" },
	};

	const std::map<Switch::Option, std::string> Switch::OptionText = {
		{ Switch::Option::ON, "On" },
		{ Switch::Option::OFF, "Off" },
		{ Switch::Option::OPEN, "Open" },
		{ Switch::Option::CLOSE, "Close" },
		{ Switch::Option::STOP, "Stop" },
		{ Switch::Option::START, "Start" },
		{ Switch::Option::IDLE, "Idle" },
		{ Switch::Option::ACTIVATE, "Activate" },
	};
	
	Switch::Switch( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) : Device( hardware_, id_, reference_, label_, enabled_ ) {
		try {
			std::string value = g_database->getQueryValue<std::string>(
				"SELECT `value` "
				"FROM `device_switch_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
			this->m_value = Switch::resolveOption( value );
			if ( this->m_value == Switch::Option::ACTIVATE ) {
				this->m_value = Switch::Option::IDLE;
			}
		} catch( const Database::NoResultsException& ex_ ) {
			g_logger->log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}

		// To avoid all devices from crunching data at the same time, the tasks are started with a small time offset.
		static std::atomic<unsigned int> offset( 0 );
		offset += ( 1000 * 11 ); // 11 seconds interval
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_HOUR + ( offset % SCHEDULER_INTERVAL_HOUR ), SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, NULL, [this]( Scheduler::Task<>& ) {
			this->_purgeHistory();
		} );
	};

	void Switch::updateValue( const Device::UpdateSource& source_, const Option& value_ ) {
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
	
	void Switch::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		for ( auto optionsIt = OptionText.begin(); optionsIt != OptionText.end(); optionsIt++ ) {
			if ( optionsIt->second == value_ ) {
				return this->updateValue( source_, optionsIt->first );
			}
		}
	};

	Switch::Option Switch::getOppositeValueOption( const Switch::Option& value_ ) throw() {
		switch( value_ ) {
			case Option::ON: return Option::OFF; break;
			case Option::OFF: return Option::ON; break;
			case Option::OPEN: return Option::CLOSE; break;
			case Option::CLOSE: return Option::OPEN; break;
			case Option::STOP: return Option::START; break;
			case Option::START: return Option::STOP; break;
			case Option::IDLE: return Option::ACTIVATE; break;
			case Option::ACTIVATE: return Option::IDLE; break;
			default: return value_; break;
		}
	};

	Switch::t_value Switch::getOppositeValue( const Switch::t_value& value_ ) throw() {
		return Switch::resolveOption( Switch::getOppositeValueOption( Switch::resolveOption( value_ ) ) );
	};

	json Switch::getJson( bool full_ ) const {
		json result = Device::getJson( full_ );
		result["value"] = this->getValue();
		result["type"] = "switch";
		
		std::string subtype = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "" ) );
		result["subtype"] = subtype;
		if ( subtype == resolveSubType( Switch::SubType::BLINDS ) ) {
			result["inverted"] = this->m_settings->get( "inverted", false );
		}
		if ( this->m_settings->contains( "rate_limit" ) ) {
			result["rate_limit"] = this->m_settings->get<double>( "rate_limit" );
		}
	
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		
		return result;
	};

	json Switch::getSettingsJson() const {
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
			for ( auto subTypeIt = Switch::SubTypeText.begin(); subTypeIt != Switch::SubTypeText.end(); subTypeIt++ ) {
				json option = {
					{ "value", subTypeIt->second },
					{ "label", subTypeIt->second }
				};
				if ( subTypeIt->first == Switch::SubType::BLINDS ) {
					option["settings"] = {
						{
							{ "name", "inverted" },
							{ "label", "Inverted" },
							{ "type", "boolean" },
							{ "sort", 11 }
						}
					};
				}
				setting["options"] += option;
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

	json Switch::getData( unsigned int range_, const std::string& interval_ ) const {
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
			"SELECT `value`, CAST( strftime('%%s',`date`) AS INTEGER ) AS `timestamp` "
			"FROM `device_switch_history` "
			"WHERE `device_id`=%d "
			"AND `date` >= datetime('now','-%d %s') "
			"ORDER BY `date` ASC ",
			this->m_id,
			range_,
			interval.c_str()
		);
	};

	void Switch::_processValue( const Device::UpdateSource& source_, const Option& value_ ) {

		// Make a local backup of the original value (the hardware might want to revert it).
		Option previous = this->m_value;
		this->m_value = value_;
		
		// If the update originates from the hardware, do not send it to the hardware again.
		bool success = true;
		bool apply = true;
		if ( ( source_ & Device::UpdateSource::HARDWARE ) != Device::UpdateSource::HARDWARE ) {
			success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		}
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_switch_history` (`device_id`, `value`) "
				"VALUES (%d, %Q)",
				this->m_id,
				Switch::resolveOption( this->m_value ).c_str()
			);
			this->m_previousValue = previous; // before newEvent so previous value can be provided
			if ( this->isEnabled() ) {
				g_controller->newEvent<Switch>( std::static_pointer_cast<Switch>( this->shared_from_this() ), source_ );
			}
			if ( this->m_value == Switch::Option::ACTIVATE ) {
				this->m_value = Switch::Option::IDLE;
				g_logger->log( this->isEnabled() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, this, "Activated." );
			} else {
				g_logger->logr( this->isEnabled() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, this, "New value %s.", Switch::OptionText.at( this->m_value ).c_str() );
			}
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

	void Switch::_purgeHistory() const {
		g_database->putQuery(
			"DELETE FROM `device_switch_history` "
			"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
			, this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
		);
	};

}; // namespace micasa
