#include "Text.h"

#include "../Database.h"
#include "../Hardware.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;

	std::string Text::toString() const {
		return this->m_name;
	};

	void Text::start() {
		this->m_value = g_database->getQueryValue(
			"SELECT `value` "
			"FROM `device_text_history` "
			"WHERE `device_id`=%q "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id.c_str()
		);
		this->m_hardware->deviceUpdated( Device::UpdateSource::INIT, this->shared_from_this() );
		Device::start();
	};

	void Text::updateValue( Device::UpdateSource source_, std::string value_ ) {
		this->m_value = value_;
		g_database->putQuery(
			"INSERT INTO `device_text_history` (`device_id`, `value`) "
			"VALUES (%q, %Q)"
			, this->m_id.c_str(), value_.c_str()
		);
		this->m_hardware->deviceUpdated( source_, this->shared_from_this() );
		
		g_webServer->touchResourceAt( "api/hardware/" + this->m_hardware->getId() + "/devices/" + this->m_id );
		g_webServer->touchResourceAt( "api/devices/" + this->m_id );

		g_logger->log( Logger::LogLevel::NORMAL, this, "New value %s.", value_.c_str() );
	}

	void Text::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		output_["value"] = this->m_value;
		Device::handleResource( resource_, code_, output_ );
	};

	std::chrono::milliseconds Text::_work( unsigned long int iteration_ ) {
		// Purge history after a configured period (defaults to 31 days for text devices because these
		// lack a separate trends table).
		g_database->putQuery(
			"DELETE FROM `device_text_history` "
			"WHERE `device_id`=%q AND `Date` < datetime('now','-%q day')"
			, this->m_id.c_str(), this->m_settings[{ "keep_history_period", "31" }].c_str()
		);
		
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	}
	
}; // namespace micasa
