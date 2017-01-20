#include <ctime>

#include "SolarEdge.h"
#include "../Database.h"
#include "../Controller.h"
#include "../Utils.h"
#include "../User.h"

#include "json.hpp"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Network> g_network;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;
	
	using namespace nlohmann;

	SolarEdge::SolarEdge( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		// The settings for SolarEdge need to be entered before the hardware is started. Therefore the
		// resource handler needs to be installed upon construction time. The resource will be destroyed by
		// the controller which uses the same identifier for specific hardware resources.
		g_webServer->addResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "$",
			99,
			User::Rights::INSTALLER,
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				auto settings = extractSettingsFromJson( input_ );
				try {
					this->m_settings->put( "api_key", settings.at( "api_key" ) );
				} catch( std::out_of_range exception_ ) { };
				try {
					this->m_settings->put( "site_id", settings.at( "site_id" ) );
				} catch( std::out_of_range exception_ ) { };
				if ( this->m_settings->isDirty() ) {
					this->m_settings->commit();
					this->m_needsRestart = true;
				}
			} )
		} );
	};
	
	void SolarEdge::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	};
	
	void SolarEdge::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	};

	json SolarEdge::getJson( bool full_ ) const {
		if ( full_ ) {
			json result = Hardware::getJson( full_ );
			result["settings"] = {
				{
					{ "name", "api_key" },
					{ "label", "API Key" },
					{ "type", "string" },
					{ "value", this->m_settings->get( "api_key", "" ) }
				},
				{
					{ "name", "site_id" },
					{ "label", "Site ID" },
					{ "type", "string" },
					{ "value", this->m_settings->get( "site_id", "" ) }
				}
			};
			return result;
		} else {
			return Hardware::getJson( full_ );
		}
	};
	
	std::chrono::milliseconds SolarEdge::_work( const unsigned long int& iteration_ ) {
		
		if ( ! this->m_settings->contains( { "api_key", "site_id" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings->get( "site_id" ) << "/list?api_key=" << this->m_settings->get( "api_key" );
		
		g_network->connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REPLY ) {
				this->_processHttpReply( connection_, (http_message*)data_ );
			} else if (
				event_ == MG_EV_CLOSE
				&& this->getState() == Hardware::State::INIT
			) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Connection failure." );
				this->setState( Hardware::State::FAILED );
			}
		} ) );
		
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	};

	void SolarEdge::_processHttpReply( mg_connection* connection_, const http_message* message_ ) {
		std::string body;
		body.assign( message_->body.p, message_->body.len );
		
		try {
			json data = json::parse( body );
			if (
				! data["reporters"].empty()
				&& ! data["reporters"]["count"].empty()
				&& data["reporters"]["count"].get<unsigned int>() >= 1
			) {
				json list = data["reporters"]["list"];
				for ( auto inverterIt = list.begin(); inverterIt != list.end(); inverterIt++ ) {
					std::stringstream label;
					label << (*inverterIt)["manufacturer"].get<std::string>() << " " << (*inverterIt)["model"].get<std::string>();
					//label << " (" << (*inverterIt)["serialNumber"].get<std::string>() << ")";
					
					g_controller->declareHardware(
						Hardware::Type::SOLAREDGE_INVERTER,
						(*inverterIt)["serialNumber"].get<std::string>(),
						this->shared_from_this(),
						{
							{ "api_key", this->m_settings->get( "api_key" ) },
							{ "site_id", this->m_settings->get( "site_id" ) },
							{ "serial", (*inverterIt)["serialNumber"].get<std::string>() },
							{ "label", label.str() }
						},
						true // auto start
					);
				}
				
				this->setState( Hardware::State::READY );
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
			this->setState( Hardware::State::FAILED );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	};
	
}; // namespace micasa
