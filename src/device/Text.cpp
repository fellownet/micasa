#include "Text.h"

#include "../Database.h"
#include "../Hardware.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;

	void Text::start() {
		this->m_value = g_database->getQueryValue(
			"SELECT `value` "
			"FROM `device_text_history` "
			"WHERE `device_id`=%q "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id.c_str()
		);
		Device::start();
	};

	bool Text::updateValue( const Device::UpdateSource source_, const std::string value_ ) {
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

			g_webServer->touchResourceAt( "api/devices/" + this->m_id );
			g_webServer->touchResourceAt( "api/devices" );

			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", value_.c_str() );
		} else {
			this->m_value = currentValue;
		}
		return success;
	}

	void Text::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		output_["value"] = this->m_value;
		Device::handleResource( resource_, code_, output_ );
	};

	std::chrono::milliseconds Text::_work( const unsigned long int iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for text devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_text_history` "
				"WHERE `device_id`=%q AND `Date` < datetime('now','-%q day')"
				, this->m_id.c_str(), this->m_settings[{ "keep_history_period", "31" }].c_str()
			);
		}
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	}
	
}; // namespace micasa
