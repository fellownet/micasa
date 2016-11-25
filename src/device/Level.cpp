#include "Level.h"

#include "../Database.h"
#include "../Hardware.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	
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

		g_webServer->addResource( new WebServer::Resource( {
			"device-" + this->m_id,
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string uri_, int& code_, nlohmann::json& output_ ) {
				output_ += {
					{ "id", atoi( this->m_id.c_str() ) },
					{ "name", this->m_name },
					{ "value", this->m_value }
				};
			} )
		} ) );
		g_webServer->addResource( new WebServer::Resource( {
			"device-" + this->m_id,
			"api/devices/" + this->m_id,
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string uri_, int& code_, nlohmann::json& output_ ) {
				output_["id"] = atoi( this->m_id.c_str() );
				output_["name"] = this->m_name;
				output_["value"] = this->m_value;
				output_["trends"] = g_database->getQuery( "SELECT `date`, `min`, `max`, `average` FROM `device_level_trends` WHERE `device_id`=%q ORDER BY `date` ASC LIMIT 48", this->m_id.c_str() );
			} )
		} ) );

		Device::start();
	};
	
	void Level::stop() {
		g_webServer->removeResource( "device-" + this->m_id ); // gets freed by webserver
		Device::stop();
	};

	bool Level::updateValue( const Device::UpdateSource source_, const float value_ ) {
		bool apply = true;
		float currentValue = this->m_value;
		this->m_value = value_;
		bool success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_level_history` (`device_id`, `value`) "
				"VALUES (%q, %.3f)"
				, this->m_id.c_str(), value_
			);
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %.3f.", value_ );
		} else {
			this->m_value = currentValue;
		}
		return success;
	};

	std::chrono::milliseconds Level::_work( const unsigned long int iteration_ ) {
		if ( iteration_ > 0 ) {
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
		}
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	};

}; // namespace micasa
