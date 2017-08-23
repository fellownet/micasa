#include "Switch.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Plugin.h"
#include "../Controller.h"
#include "../Utils.h"

#define DEVICE_SWITCH_DEFAULT_HISTORY_RETENTION 7 // days

namespace micasa {

	extern std::unique_ptr<Database> g_database;
	extern std::unique_ptr<Controller> g_controller;

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
		{ Switch::SubType::SCENE, "scene" },
		{ Switch::SubType::OCCUPANCY, "occupancy" },
		{ Switch::SubType::ACTION, "action" },
	};

	const std::map<Switch::Option, std::string> Switch::OptionText = {
		{ Switch::Option::ON, "On" },
		{ Switch::Option::OFF, "Off" },
		{ Switch::Option::OPEN, "Open" },
		{ Switch::Option::CLOSE, "Close" },
		{ Switch::Option::STOP, "Stop" },
		{ Switch::Option::START, "Start" },
		{ Switch::Option::ENABLED, "Enabled" },
		{ Switch::Option::DISABLED, "Disabled" },
		{ Switch::Option::IDLE, "Idle" },
		{ Switch::Option::ACTIVATE, "Activate" },
	};

	Switch::Switch( std::weak_ptr<Plugin> plugin_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) :
		Device( plugin_, id_, reference_, label_, enabled_ ),
		m_value( Option::OFF ),
		m_updated( system_clock::now() ),
		m_rateLimiter( { Option::OFF, Device::resolveUpdateSource( 0 ) } )
	{
		try {
			json result = g_database->getQueryRow<json>(
				"SELECT `value`, CAST( strftime( '%%s', 'now' ) AS INTEGER ) - CAST( strftime( '%%s', `date` ) AS INTEGER ) AS `age` "
				"FROM `device_switch_history` "
				"WHERE `device_id` = %d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
			this->m_value = this->m_rateLimiter.value = Switch::resolveTextOption( jsonGet<std::string>( result, "value" ) );
			this->m_updated = system_clock::now() - seconds( jsonGet<unsigned int>( result, "age" ) );
		} catch( const Database::NoResultsException& ex_ ) {
			Logger::log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}
	};

	void Switch::start() {
#ifdef _DEBUG
		assert( this->m_enabled && "Device needs to be enabled while being started." );
#endif // _DEBUG
		this->m_scheduler.schedule( randomNumber( 0, SCHEDULER_INTERVAL_1HOUR ), SCHEDULER_INTERVAL_1HOUR, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			this->_purgeHistory();
		} );
	};

	void Switch::stop() {
#ifdef _DEBUG
		assert( this->m_enabled && "Device needs to be enabled while being stopped." );
#endif // _DEBUG
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
	};

	void Switch::updateValue( Device::UpdateSource source_, Option value_ ) {
		Switch::SubType subType = Switch::resolveTextSubType( this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "generic" ) ) );
		if (
			! this->m_enabled
			&& subType != Switch::SubType::ACTION
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

	void Switch::updateValue( Device::UpdateSource source_, t_value value_ ) {
		for ( auto optionsIt = OptionText.begin(); optionsIt != OptionText.end(); optionsIt++ ) {
			if ( optionsIt->second == value_ ) {
				return this->updateValue( source_, optionsIt->first );
			}
		}
		Logger::logr( Logger::LogLevel::ERROR, this, "Invalid value %s.", value_.c_str() );
	};

	Switch::Option Switch::getOppositeValueOption( const Switch::Option& value_ ) {
		switch( value_ ) {
			case Option::ON: return Option::OFF; break;
			case Option::OFF: return Option::ON; break;
			case Option::OPEN: return Option::CLOSE; break;
			case Option::CLOSE: return Option::OPEN; break;
			case Option::STOP: return Option::START; break;
			case Option::START: return Option::STOP; break;
			case Option::ENABLED: return Option::DISABLED; break;
			case Option::DISABLED: return Option::ENABLED; break;
			case Option::IDLE: return Option::ACTIVATE; break;
			case Option::ACTIVATE: return Option::IDLE; break;
			default: return value_; break;
		}
	};

	Switch::t_value Switch::getOppositeValue( const Switch::t_value& value_ ) {
		return Switch::resolveTextOption( Switch::getOppositeValueOption( Switch::resolveTextOption( value_ ) ) );
	};

	json Switch::getJson() const {
		json result = Device::getJson();

		result["value"] = this->getValue();
		result["source"] = Device::resolveUpdateSource( this->m_source );
		result["age"] = duration_cast<seconds>( system_clock::now() - this->m_updated ).count();
		result["type"] = "switch";
		std::string subtype = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "generic" ) );
		result["subtype"] = subtype;
		if ( subtype == resolveTextSubType( Switch::SubType::BLINDS ) ) {
			result["inverted"] = this->m_settings->get( "inverted", false );
		}
		result["history_retention"] = this->m_settings->get<int>( "history_retention", DEVICE_SWITCH_DEFAULT_HISTORY_RETENTION );
		if ( this->m_settings->contains( "rate_limit" ) ) {
			result["rate_limit"] = this->m_settings->get<double>( "rate_limit" );
		}

		for ( auto const& plugin : g_controller->getAllPlugins() ) {
			plugin->updateDeviceJson( Device::shared_from_this(), result, plugin == this->getPlugin() );
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
			{ "name", "history_retention" },
			{ "label", "History Retention" },
			{ "description", "How long to keep history in the database in days. Switch devices store each collected value in the history database." },
			{ "type", "int" },
			{ "minimum", 1 },
			{ "default", DEVICE_SWITCH_DEFAULT_HISTORY_RETENTION },
			{ "class", "advanced" },
			{ "sort", 12 }
		};

		result += {
			{ "name", "rate_limit" },
			{ "label", "Rate Limiter" },
			{ "description", "Limits the number of updates to once per configured time period in seconds." },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 999 }
		};

		for ( auto const& plugin : g_controller->getAllPlugins() ) {
			plugin->updateDeviceSettingsJson( Device::shared_from_this(), result, plugin == this->getPlugin() );
		}

		return result;
	};

	json Switch::getData( unsigned int range_, const std::string& interval_ ) const {
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
			"FROM `device_switch_history` "
			"WHERE `device_id` = %d "
			"AND `date` >= datetime( 'now', '-%d %s' ) "
			"ORDER BY `date` ASC ",
			this->m_id,
			range_,
			interval.c_str()
		);
	};

	void Switch::_processValue( const Device::UpdateSource& source_, const Option& value_ ) {

		// Make a local backup of the original value (the plugin might want to revert it).
		Option previous = this->m_value;
		this->m_value = value_;

		// If the update originates from the plugin it is not send back to the plugin again.
		bool success = true;
		bool apply = true;
		for ( auto const& plugin : g_controller->getAllPlugins() ) {
			if (
				plugin != this->getPlugin()
				|| ( source_ & Device::UpdateSource::PLUGIN ) != Device::UpdateSource::PLUGIN
			) {
				success = success && plugin->updateDevice( source_, Device::shared_from_this(), plugin == this->getPlugin(), apply );
			}
		}
		if ( success && apply ) {
			if ( this->m_enabled ) {
				g_database->putQuery(
					"INSERT INTO `device_switch_history` ( `device_id`, `value` ) "
					"VALUES ( %d, %Q )",
					this->m_id,
					Switch::resolveTextOption( this->m_value ).c_str()
				);
			}
			this->m_source = source_;
			this->m_updated = system_clock::now();
			if (
				this->getPlugin()->getState() >= Plugin::State::READY
				&& (
					this->m_enabled
					|| Switch::SubType::ACTION == Switch::resolveTextSubType( this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "generic" ) ) )
				)
			) {
				g_controller->newEvent<Switch>( std::static_pointer_cast<Switch>( Device::shared_from_this() ), source_ );
			}
			if ( this->m_value == Switch::Option::ACTIVATE ) {
				Logger::log( Logger::LogLevel::NORMAL, this, "Activated." );
				this->m_scheduler.schedule( SCHEDULER_INTERVAL_5SEC, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
					this->_processValue( Device::UpdateSource::SYSTEM | Device::UpdateSource::PLUGIN, Switch::Option::IDLE );
				} );
			} else {
				Logger::logr( Logger::LogLevel::NORMAL, this, "New value %s.", Switch::OptionText.at( this->m_value ).c_str() );
			}
		} else {
			this->m_value = previous;
		}
	};

	void Switch::_purgeHistory() const {
		g_database->putQuery(
			"DELETE FROM `device_switch_history` "
			"WHERE `device_id` = %d "
			"AND `date` < datetime( 'now','-%d day' )",
			this->m_id,
			this->m_settings->get<int>( "history_retention", DEVICE_SWITCH_DEFAULT_HISTORY_RETENTION )
		);
	};

}; // namespace micasa
