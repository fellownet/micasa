#include "Text.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	const Device::Type Text::type = Device::Type::TEXT;

	const std::map<Text::SubType, std::string> Text::SubTypeText = {
		{ Text::SubType::GENERIC, "generic" },
		{ Text::SubType::WIND_DIRECTION, "wind_direction" }
	};

	void Text::start() {
		try {
			this->m_value = g_database->getQueryValue<std::string>(
				"SELECT `value` "
				"FROM `device_text_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1"
				, this->m_id
			);
		} catch( const Database::NoResultsException& ex_ ) { }

		Device::start();
	};

	void Text::stop() {
		Device::stop();
	};
	
	bool Text::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		
		// The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}

		// Make a local backup of the original value (the hardware might want to revert it).
		t_value previous = this->m_value;
		this->m_value = value_;
		
		// If the update originates from the hardware, do not send it to the hardware again!
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
				value_.c_str()
			);
			this->m_previousValue = previous; // before newEvent so previous value can be provided
			if ( this->isRunning() ) {
				g_controller->newEvent<Text>( *this, source_ );
			}
			this->m_lastUpdate = std::chrono::system_clock::now(); // after newEvent so the interval can be determined
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", value_.c_str() );
		} else {
			this->m_value = previous;
		}
		return success;
	};

	json Text::getJson( bool full_ ) const {
		json result = Device::getJson();
		result["value"] = this->getValue();
		result["type"] = "text";
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
				for ( auto subTypeIt = Text::SubTypeText.begin(); subTypeIt != Text::SubTypeText.end(); subTypeIt++ ) {
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
	
	std::chrono::milliseconds Text::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for text devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_text_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
			return std::chrono::milliseconds( 1000 * 60 * 60 );
		} else {
			// To prevent all devices from crunching data at the same time an offset is used.
			static volatile unsigned int offset = 0;
			offset += ( 1000 * 25 ); // 25 seconds interval
			return std::chrono::milliseconds( offset % ( 1000 * 60 * 5 ) );
		}
	};
	
}; // namespace micasa
