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

		this->m_task = this->m_scheduler.schedule( 0, 1, this, "solaredge", [this]( Scheduler::Task<>& ) {
			if ( ! this->m_settings->contains( { "api_key", "site_id" } ) ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( FAILED );
				this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, this->m_task );
				return;
			}

			std::stringstream url;
			url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings->get( "site_id" ) << "/list?api_key=" << this->m_settings->get( "api_key" );
		
			// A weak pointer to this is captured into the connection handler to make sure the handler doesn't keep the
			// hardware from being destroyed by the controller.
			std::weak_ptr<SolarEdge> ptr = std::static_pointer_cast<SolarEdge>( this->shared_from_this() );
			Network::get().connect( url.str(), Network::t_callback( [ptr]( mg_connection* connection_, int event_, void* data_ ) {
				auto me = ptr.lock();
				if ( me ) {
					if ( event_ == MG_EV_HTTP_REPLY ) {
						std::string body;
						body.assign( ((http_message*)data_)->body.p, ((http_message*)data_)->body.len );
						if ( ! me->_process( body ) ) {
							me->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, me->m_task );	
						}
						connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
					} else if (
						event_ == MG_EV_CLOSE
						&& me->getState() == INIT
					) {
						g_logger->log( Logger::LogLevel::ERROR, me, "Connection failure." );
						me->setState( FAILED );
						me->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, me->m_task );
					}
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
		Hardware::State state = this->getState();
		result += {
			{ "name", "api_key" },
			{ "label", "API Key" },
			{ "type", "string" },
			{ "class", state == READY || state == SLEEPING ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 98 }
		};
		result += {
			{ "name", "site_id" },
			{ "label", "Site ID" },
			{ "type", "string" },
			{ "class", state == READY || state == SLEEPING ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};

	bool SolarEdge::_process( const std::string& body_ ) {
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
				this->setState( READY );
				this->m_scheduler.schedule( 1000 * 10, 1, NULL, "solaredge retry", [this]( Scheduler::Task<>& ) -> void {
					this->setState( SLEEPING );
				} );
				return true;
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
			this->setState( FAILED );
		}
		return false;
	};
	
}; // namespace micasa
