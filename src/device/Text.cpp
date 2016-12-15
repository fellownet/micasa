#include "Text.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	void Text::start() {
		this->m_value = g_database->getQueryValue<std::string>(
			"SELECT `value` "
			"FROM `device_text_history` "
			"WHERE `device_id`=%d "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id
		);

		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( output_.is_null() ) {
					output_ = nlohmann::json::array();
				}
				//auto inputIt = input_.find( "hardware_id" );
				//if (
				//	inputIt == input_.end()
				//	|| (*inputIt).second == std::to_string( this->m_hardware->getId() )
				//) {
					output_ += this->getJson();
				//}
			} )
		} ) ) );
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices/" + std::to_string( this->m_id ),
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				output_ = this->getJson();
			} )
		} ) ) );

		Device::start();
	};

	void Text::stop() {
		g_webServer->removeResourceCallback( "device-" + std::to_string( this->m_id ) );
		Device::stop();
	};
	
	bool Text::updateValue( const unsigned int& source_, const t_value& value_ ) {
		
		// The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}

		// Make a local backup of the original value (the hardware might want to revert it).
		t_value currentValue = this->m_value;
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
				"VALUES (%d, %Q)"
				, this->m_id, value_.c_str()
			);
			g_controller->newEvent<Text>( *this, source_ );
			g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
			this->m_lastUpdate = std::chrono::system_clock::now(); // after newEvent so the interval can be determined
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", value_.c_str() );
		} else {
			this->m_value = currentValue;
		}
		return success;
	};

	json Text::getJson() const {
		json result = Device::getJson();
		result["value"] = this->getValue();
		result["type"] = "text";
		return result;
	};
	
	const std::chrono::milliseconds Text::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for text devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_text_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings.get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
		}
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	};
	
}; // namespace micasa
