#include "Counter.h"

#include "../Database.h"
#include "../Hardware.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;

	void Counter::start() {
		auto databaseValue = g_database->getQueryValue(
		   "SELECT `value` "
		   "FROM `device_counter_history` "
		   "WHERE `device_id`=%q "
		   "ORDER BY `date` DESC "
		   "LIMIT 1"
		   , this->m_id.c_str()
		);
		this->m_value = atoi( databaseValue.c_str() );
		Device::start();
	};

	bool Counter::updateValue( const Device::UpdateSource source_, const int value_ ) {
		bool apply = true;
		int currentValue = this->m_value;
		this->m_value = value_;
		bool success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_counter_history` (`device_id`, `value`) "
				"VALUES (%q, %d)"
				, this->m_id.c_str(), value_
			);
			
			g_webServer->touchResourceAt( "api/devices/" + this->m_id );
			g_webServer->touchResourceAt( "api/devices" );
			
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %d.", value_ );
		} else {
			this->m_value = currentValue;
		}
		return success;
	}
	
	void Counter::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		output_["value"] = this->m_value;
		Device::handleResource( resource_, code_, output_ );
	};
	
	std::chrono::milliseconds Counter::_work( const unsigned long int iteration_ ) {
		if ( iteration_ > 0 ) {
			std::string hourFormat = "%Y-%m-%d %H:00:00";
			std::string groupFormat = "%Y-%m-%d-%H";
			// TODO do not generate trends for anything older than an hour?? and what if micasa was offline for a while?
			// TODO maybe grab the latest hour from trends and start from there?
			// TODO for counter devices it's no use storing the latest value of the current hour in trends, only process
			// full (completed) hours.
			auto trends = g_database->getQuery(
				"SELECT MAX(`date`) AS `date1`, strftime(%Q, MAX(`date`)) AS `date2`, MAX(`value`)-MIN(`value`) AS `diff` "
				"FROM `device_counter_history` "
				"WHERE `device_id`=%q AND `Date` > datetime('now','-1 hour') "
				"GROUP BY strftime(%Q, `date`)"
				, hourFormat.c_str(), this->m_id.c_str(), groupFormat.c_str()
			);
			for ( auto trendsIt = trends.begin(); trendsIt != trends.end(); trendsIt++ ) {
				auto value = g_database->getQueryValue(
					"SELECT `value` "
					"FROM `device_counter_history` "
					"WHERE `device_id`=%q AND `date`=%Q"
					, this->m_id.c_str(), (*trendsIt)["date1"].c_str()
				);
				g_database->putQuery(
					"REPLACE INTO `device_counter_trends` (`device_id`, `last`, `diff`, `date`) "
					"VALUES (%q, %q, %q, %Q)"
					, this->m_id.c_str(), value.c_str(), (*trendsIt)["diff"].c_str(), (*trendsIt)["date2"].c_str()
				);
			}

			// Purge history after a configured period (defaults to 7 days for counter devices because these have a
			// separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_counter_history` "
				"WHERE `device_id`=%q AND `Date` < datetime('now','-%q day')"
				, this->m_id.c_str(), this->m_settings[{ "keep_history_period", "7" }].c_str()
			);
		}
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	}

}; // namespace micasa
