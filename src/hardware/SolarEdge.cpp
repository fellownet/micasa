#include <ctime>

#include "SolarEdge.h"

#include "../Logger.h"
#include "../Network.h"
#include "../Controller.h"

#include "json.hpp"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	
	using namespace nlohmann;

	void SolarEdge::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		this->m_scheduler.schedule( 0, SCHEDULER_INTERVAL_HOUR, SCHEDULER_INFINITE, this, [this]( Scheduler::Task<>& ) {
			if ( ! this->m_settings->contains( { "api_key", "site_id" } ) ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Hardware::State::FAILED );
				return;
			}

			std::stringstream url;
			url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings->get( "site_id" ) << "/list?api_key=" << this->m_settings->get( "api_key" );
			
			Network::get().connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
				if ( event_ == MG_EV_HTTP_REPLY ) {
					std::string body;
					body.assign( ((http_message*)data_)->body.p, ((http_message*)data_)->body.len );
					this->_process( body );
					connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
				} else if (
					event_ == MG_EV_CLOSE
					&& this->getState() == Hardware::State::INIT
				) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Connection failure." );
					this->setState( Hardware::State::FAILED );
				}
			} ) );
		} );
	};
	
	void SolarEdge::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		Hardware::stop();
	};

	json SolarEdge::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["api_key"] = this->m_settings->get( "api_key", "" );
		result["site_id"] = this->m_settings->get( "site_id", "" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json SolarEdge::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		result += {
			{ "name", "api_key" },
			{ "label", "API Key" },
			{ "type", "string" },
			{ "class", this->getState() == Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 98 }
		};
		result += {
			{ "name", "site_id" },
			{ "label", "Site ID" },
			{ "type", "string" },
			{ "class", this->getState() == Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};

	void SolarEdge::_process( const std::string& body_ ) {
		try {
			json data = json::parse( body_ );
			if (
				! data["reporters"].empty()
				&& ! data["reporters"]["count"].empty()
				&& data["reporters"]["count"].get<unsigned int>() >= 1
			) {
				json list = data["reporters"]["list"];
				for ( auto inverterIt = list.begin(); inverterIt != list.end(); inverterIt++ ) {
					std::stringstream label;
					label << (*inverterIt)["manufacturer"].get<std::string>() << " " << (*inverterIt)["model"].get<std::string>();
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
						true
					)->start();
				}
				this->setState( Hardware::State::READY );
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
			this->setState( Hardware::State::FAILED );
		}
	};
	
}; // namespace micasa
