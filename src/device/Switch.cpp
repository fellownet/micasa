#include "Switch.h"

#include "../Database.h"
#include "../Hardware.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;

	const std::map<int, std::string> Switch::OptionsText = {
		{ Switch::Options::ON, "On" },
		{ Switch::Options::OFF, "Off" },
		{ Switch::Options::OPEN, "Open" },
		{ Switch::Options::CLOSED, "Closed" },
		{ Switch::Options::STOPPED, "Stopped" },
		{ Switch::Options::STARTED, "Started" },
	};
	
	std::string Switch::toString() const {
		return this->m_name;
	};

	void Switch::start() {
		std::string value = g_database->getQueryValue(
			"SELECT `value` "
			"FROM `device_switch_history` "
			"WHERE `device_id`=%q "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id.c_str()
		);
		this->m_value = atoi( value.c_str() );
		this->m_hardware->deviceUpdated( Device::UpdateSource::INIT, this->shared_from_this() );
		Device::start();
	};

	void Switch::updateValue( Device::UpdateSource source_, Options value_ ) {
#ifdef _DEBUG
		assert( Switch::OptionsText.find( value_ ) != Switch::OptionsText.end() && "Switch should be defined." );
#endif // _DEBUG
		this->m_value = value_;
		g_database->putQuery(
			"INSERT INTO `device_switch_history` (`device_id`, `value`) "
			"VALUES (%q, %d)"
			, this->m_id.c_str(), (unsigned int)value_
		);
		this->m_hardware->deviceUpdated( source_, this->shared_from_this() );
		
		g_webServer->touchResourceAt( "api/hardware/" + this->m_hardware->getId() + "/devices/" + this->m_id );
		g_webServer->touchResourceAt( "api/devices/" + this->m_id );

		g_logger->log( Logger::LogLevel::NORMAL, this, "New value %d.", (unsigned int)value_ );
	}

	void Switch::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		output_["value"] = Switch::OptionsText.at( this->m_value );
		Device::handleResource( resource_, code_, output_ );
	};

	std::chrono::milliseconds Switch::_work( unsigned long int iteration_ ) {
		// Purge history after a configured period (defaults to 31 days for switch devices because these
		// lack a separate trends table).
		g_database->putQuery(
			"DELETE FROM `device_switch_history` "
			"WHERE `device_id`=%q AND `Date` < datetime('now','-%q day')"
			, this->m_id.c_str(), this->m_settings[{ "keep_history_period", "31" }].c_str()
		);
		
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	}
	
}; // namespace micasa
