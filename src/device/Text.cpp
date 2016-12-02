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
			"WHERE `device_id`=%q "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id.c_str()
		);

		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + this->m_id,
			"Returns a list of available devices.",
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string uri_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				output_ += {
					{ "id", atoi( this->m_id.c_str() ) },
					{ "name", this->m_name },
					{ "value", this->m_value }
				};
			} )
		} ) ) );
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + this->m_id,
			"Returns detailed information for " + this->m_name,
			"api/devices/" + this->m_id,
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string uri_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				output_["id"] = atoi( this->m_id.c_str() );
				output_["name"] = this->m_name;
				output_["value"] = this->m_value;
			} )
		} ) ) );

		Device::start();
	};

	void Text::stop() {
		g_webServer->removeResourceCallback( "device-" + this->m_id );
		Device::stop();
	};
	
	bool Text::updateValue( const Device::UpdateSource source_, const std::string value_ ) {
		// The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}

		bool apply = true;
		std::string currentValue = this->m_value;
		this->m_value = value_;
		bool success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_text_history` (`device_id`, `value`) "
				"VALUES (%q, %Q)"
				, this->m_id.c_str(), value_.c_str()
			);
			g_webServer->touchResourceAt( "api/devices" );
			g_webServer->touchResourceAt( "api/devices/" + this->m_id );
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", value_.c_str() );
		} else {
			this->m_value = currentValue;
		}
		return success;
	}

	std::chrono::milliseconds Text::_work( const unsigned long int iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for text devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_text_history` "
				"WHERE `device_id`=%q AND `Date` < datetime('now','-%d day')"
				, this->m_id.c_str(), this->m_settings.get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
		}
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	}
	
}; // namespace micasa
