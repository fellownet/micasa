#include "Level.h"

#include "../Database.h"
#include "../Hardware.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	
	std::string Level::toString() const {
		return this->m_name;
	};

	void Level::start() {
		auto databaseValue = g_database->getQueryValue(
			"SELECT `value` "
			"FROM `device_level_history` "
			"WHERE `device_id`=%q "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id.c_str()
		);
		this->m_value = atof( databaseValue.c_str() );
		this->m_hardware->deviceUpdated( Device::UpdateSource::INIT, this->shared_from_this() );
		Device::start();
	};
	
	void Level::updateValue( Device::UpdateSource source_, float value_ ) {
		this->m_value = value_;
		g_database->putQuery(
			"INSERT INTO `device_level_history` (`device_id`, `value`) "
			"VALUES (%q, %.3f)"
			, this->m_id.c_str(), value_
		);
		this->m_hardware->deviceUpdated( source_, this->shared_from_this() );

		g_webServer->touchResourceAt( "api/hardware/" + this->m_hardware->getId() + "/devices/" + this->m_id );
		g_webServer->touchResourceAt( "api/devices/" + this->m_id );
		
		g_logger->log( Logger::LogLevel::NORMAL, this, "New value %.3f.", value_ );
	};

	void Level::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		output_["value"] = this->m_value;
		Device::handleResource( resource_, code_, output_ );
	};
	
	std::chrono::milliseconds Level::_work( unsigned long int iteration_ ) {
		std::string hourFormat = "%Y-%m-%d %H:00:00";
		std::string groupFormat = "%Y-%m-%d-%H";
		// TODO do not generate trends for anything older than an hour?? and what if micasa was offline for a while?
		// TODO maybe grab the latest hour from trends and start from there?
		auto trends = g_database->getQuery(
			"SELECT MAX(`value`) AS `max`, MIN(`value`) AS `min`, printf(\"%%.3f\", AVG(`value`)) AS `average`, strftime(%Q, MAX(`date`)) AS `date` "
			"FROM `device_level_history` "
			"WHERE `device_id`=%q AND `Date` > datetime('now','-1 hour') "
			"GROUP BY strftime(%Q, `date`)"
			, hourFormat.c_str(), this->m_id.c_str(), groupFormat.c_str()
		);
		for ( auto trendsIt = trends.begin(); trendsIt != trends.end(); trendsIt++ ) {
			// TODO round these values to, say, 3 decimals?
			g_database->putQuery(
				"REPLACE INTO `device_level_trends` (`device_id`, `min`, `max`, `average`, `date`) "
				"VALUES (%q, %q, %q, %q, %Q)"
				, this->m_id.c_str(), (*trendsIt)["min"].c_str(), (*trendsIt)["max"].c_str(), (*trendsIt)["average"].c_str(), (*trendsIt)["date"].c_str()
			);
		}
		// Purge history after a configured period (defaults to 7 days for level devices because these have a
		// separate trends table).
		g_database->putQuery(
			"DELETE FROM `device_level_history` "
			"WHERE `device_id`=%q AND `Date` < datetime('now','-%q day')"
			, this->m_id.c_str(), this->m_settings[{ "keep_history_period", "7" }].c_str()
		);
		
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	};

}; // namespace micasa
