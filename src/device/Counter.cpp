#include "Counter.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	void Counter::start() {
		this->m_value = g_database->getQueryValue<int>(
		   "SELECT `value` "
		   "FROM `device_counter_history` "
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
			WebServer::t_callback( [this]( const std::string uri_, int& code_, nlohmann::json& output_ ) {
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
			WebServer::Method::GET | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const std::string uri_, int& code_, nlohmann::json& output_ ) {
				output_["id"] = atoi( this->m_id.c_str() );
				output_["name"] = this->m_name;
				output_["value"] = this->m_value;
				output_["trends"] = g_database->getQuery( "SELECT `date`, `diff`, `last` FROM `device_counter_trends` WHERE `device_id`=%q ORDER BY `date` ASC LIMIT 48", this->m_id.c_str() );
			} )
		} ) ) );

		Device::start();
	};

	void Counter::stop() {
		g_webServer->removeResourceCallback( "device-" + this->m_id );
		Device::stop();
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
			g_webServer->touchResourceAt( "api/devices" );
			g_webServer->touchResourceAt( "api/devices/" + this->m_id );
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %d.", value_ );
		} else {
			this->m_value = currentValue;
		}
		return success;
	}
	
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
				auto value = g_database->getQueryValue<std::string>(
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
