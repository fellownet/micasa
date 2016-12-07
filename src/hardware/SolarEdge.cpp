#include <ctime>

#include "SolarEdge.h"
#include "../Database.h"
#include "../Controller.h"

#include "json.hpp"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Network> g_network;
	extern std::shared_ptr<Controller> g_controller;
	
	using namespace nlohmann;
	
	void SolarEdge::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	}
	
	void SolarEdge::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	}
	
	std::chrono::milliseconds SolarEdge::_work( const unsigned long int iteration_ ) {
		
		if ( ! this->m_settings.contains( { "api_key", "site_id" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings["site_id"] << "/list?api_key=" << this->m_settings["api_key"];
		
		g_network->connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REPLY ) {
				this->_processHttpReply( connection_, (http_message*)data_ );
			}
		} ) );
		
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	}

	void SolarEdge::_processHttpReply( mg_connection* connection_, const http_message* message_ ) {
		std::string body;
		body.assign( message_->body.p, message_->body.len );
		
		try {
			json data = json::parse( body );
			if (
				! data["reporters"].empty()
				&& ! data["reporters"]["count"].empty()
				&& data["reporters"]["count"] >= 1
			) {
				json list = data["reporters"]["list"];
				for ( auto inverterIt = list.begin(); inverterIt != list.end(); inverterIt++ ) {
					std::stringstream name;
					name << (*inverterIt)["manufacturer"].get<std::string>() << " " << (*inverterIt)["model"].get<std::string>();
					//name << " (" << (*inverterIt)["serialNumber"].get<std::string>() << ")";
					
					g_controller->declareHardware(
						Hardware::HardwareType::SOLAREDGE_INVERTER,
						(*inverterIt)["serialNumber"].get<std::string>(),
						this->shared_from_this(),
						name.str(),
						{
							{ "api_key", this->m_settings["api_key"] },
							{ "site_id", this->m_settings["site_id"] },
							{ "serial", (*inverterIt)["serialNumber"].get<std::string>() }
						}
					);
				}
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	}
	
}; // namespace micasa
