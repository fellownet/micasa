#include "Switch.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	const Device::Type Switch::type = Device::Type::SWITCH;

	const std::map<Switch::SubType, std::string> Switch::SubTypeText = {
		{ Switch::SubType::GENERIC, "generic" },
		{ Switch::SubType::LIGHT, "light" },
		{ Switch::SubType::DOOR_CONTACT, "door_contact" },
		{ Switch::SubType::BLINDS, "blinds" },
		{ Switch::SubType::MOTION_DETECTOR, "motion_detector" },
	};

	const std::map<Switch::Option, std::string> Switch::OptionText = {
		{ Switch::Option::ON, "On" },
		{ Switch::Option::OFF, "Off" },
		{ Switch::Option::OPEN, "Open" },
		{ Switch::Option::CLOSED, "Closed" },
		{ Switch::Option::STOPPED, "Stopped" },
		{ Switch::Option::STARTED, "Started" },
	};
	
	void Switch::start() {
		try {
			std::string value = g_database->getQueryValue<std::string>(
				"SELECT `value` "
				"FROM `device_switch_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1"
				, this->m_id
			);
			this->m_value = Switch::resolveOption( value );
		} catch( const Database::NoResultsException& ex_ ) { }

		Device::start();
	};

	void Switch::stop() {
		Device::stop();
	};

	bool Switch::updateValue( const Device::UpdateSource& source_, const Option& value_ ) {
		
		// The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}
		
		// Make a local backup of the original value (the hardware might want to revert it).
		Option previous = this->m_value;
		this->m_value = value_;
		
		// If the update originates from the hardware, do not send it to the hardware again!
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
				Switch::resolveOption( value_ ).c_str()
			);
			this->m_previousValue = previous; // before newEvent so previous value can be provided
			if ( this->isRunning() ) {
				g_controller->newEvent<Switch>( *this, source_ );
			}
			this->m_lastUpdate = std::chrono::system_clock::now(); // after newEvent so the interval can be determined
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", Switch::OptionText.at( value_ ).c_str() );
		} else {
			this->m_value = previous;
		}
		return success;
	};
	
	bool Switch::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		for ( auto optionsIt = OptionText.begin(); optionsIt != OptionText.end(); optionsIt++ ) {
			if ( optionsIt->second == value_ ) {
				return this->updateValue( source_, optionsIt->first );
			}
		}
		return false;
	};

	json Switch::getJson( bool full_ ) const {
		json result = Device::getJson();
		result["value"] = this->getValue();
		result["type"] = "switch";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "" ) );

		if ( full_ ) {
			if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, false ) ) {
				json setting = {
					{ "name", "subtype" },
					{ "label", "SubType" },
					{ "type", "list" },
					{ "options", json::array() },
					{ "value", result["subtype"] }
				};
				for ( auto subTypeIt = Switch::SubTypeText.begin(); subTypeIt != Switch::SubTypeText.end(); subTypeIt++ ) {
					setting["options"] += {
						{ "value", subTypeIt->second },
						{ "label", subTypeIt->second }
					};
				}
				result["settings"] += setting;
			}
		}

		return result;
	};

	std::chrono::milliseconds Switch::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for switch devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_switch_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
			return std::chrono::milliseconds( 1000 * 60 * 60 );
		} else {
			// To prevent all devices from crunching data at the same time an offset is used.
			static volatile unsigned int offset = 0;
			offset += ( 1000 * 20 ); // 20 seconds interval
			return std::chrono::milliseconds( offset % ( 1000 * 60 * 5 ) );
		}
	};
	
}; // namespace micasa
