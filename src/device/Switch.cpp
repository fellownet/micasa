#include "Switch.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	const std::map<Switch::Option, std::string> Switch::OptionText = {
		{ Switch::Option::ON, "On" },
		{ Switch::Option::OFF, "Off" },
		{ Switch::Option::OPEN, "Open" },
		{ Switch::Option::CLOSED, "Closed" },
		{ Switch::Option::STOPPED, "Stopped" },
		{ Switch::Option::STARTED, "Started" },
	};
	
	void Switch::start() {
		std::string value = g_database->getQueryValue<std::string>(
			"SELECT `value` "
			"FROM `device_switch_history` "
			"WHERE `device_id`=%d "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id
		);
		this->m_value = (Option)atoi( value.c_str() );
		Device::start();
	};

	void Switch::stop() {
		Device::stop();
	};

	bool Switch::updateValue( const unsigned int& source_, const Option& value_ ) {
#ifdef _DEBUG
		assert( Switch::OptionText.find( value_ ) != Switch::OptionText.end() && "Switch should be defined." );
#endif // _DEBUG
		
	   // The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}
		
		// TODO does this make sense?
		// Prevent the same updates.
		if ( this->m_value == value_ ) {
			//return true; // update was successful but nothing has to happen
		}
		
		// Make a local backup of the original value (the hardware might want to revert it).
		Option currentValue = this->m_value;
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
				"VALUES (%d, %d)"
				, this->m_id, (unsigned int)value_
			);
			if ( this->isRunning() ) {
				g_controller->newEvent<Switch>( *this, source_ );
			}
			g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
			this->m_lastUpdate = std::chrono::system_clock::now(); // after newEvent so the interval can be determined
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", Switch::OptionText.at( value_ ).c_str() );
		} else {
			this->m_value = currentValue;
		}
		return success;
	};
	
	bool Switch::updateValue( const unsigned int& source_, const t_value& value_ ) {
		for ( auto optionsIt = OptionText.begin(); optionsIt != OptionText.end(); optionsIt++ ) {
			if ( optionsIt->second == value_ ) {
				return this->updateValue( source_, optionsIt->first );
			}
		}
		return false;
	};

	json Switch::getJson() const {
		json result = Device::getJson();
		result["value"] = this->getValue();
		result["type"] = "switch";
		// TODO also populate with possible switch options?
		return result;
	};

	const std::chrono::milliseconds Switch::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for switch devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_switch_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings.get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
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
