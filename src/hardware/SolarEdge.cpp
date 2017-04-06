#include <ctime>

#include "SolarEdge.h"

#include "../Logger.h"
#include "../Network.h"
#include "../Controller.h"

#include "json.hpp"

namespace micasa {

	using namespace nlohmann;

	const char* SolarEdge::label = "SolarEdge API";

	extern std::shared_ptr<Controller> g_controller;

	void SolarEdge::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		this->m_task = this->m_scheduler.schedule( 0, 1, this, "solaredge", [this]( Scheduler::Task<>& task_ ) {
			if ( ! this->m_settings->contains( { "api_key", "site_id" } ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Hardware::State::FAILED );
				return;
			}

			if ( this->m_connection != nullptr ) {
				this->m_connection->wait();
			}

			std::stringstream url;
			url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings->get( "site_id" ) << "/list?api_key=" << this->m_settings->get( "api_key" );
			this->m_connection = Network::connect( url.str(), [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) {
				switch( event_ ) {
					case Network::Connection::Event::CONNECT: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );
						break;
					}
					case Network::Connection::Event::FAILURE: {
						Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
						this->setState( Hardware::State::FAILED );
						this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, this->m_task );
						break;
					}
					case Network::Connection::Event::HTTP_RESPONSE: {
						if ( ! this->_process( connection_->getResponse() ) ) {
							this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, this->m_task );
						}
						break;
					}
					case Network::Connection::Event::CLOSE: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connection closed." );
						break;
					}
					default: { break; }
				}
			} );
		} );
	};
	
	void SolarEdge::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		if ( this->m_connection != nullptr ) {
			this->m_connection->wait();
		}
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
			{ "class", state >= Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 98 }
		};
		result += {
			{ "name", "site_id" },
			{ "label", "Site ID" },
			{ "type", "string" },
			{ "class", state >= Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};

	bool SolarEdge::_process( const std::string& data_ ) {
		try {
			json data = json::parse( data_ );
			if (
				! data["reporters"].empty()
				&& ! data["reporters"]["count"].empty()
				&& data["reporters"]["count"].get<unsigned int>() >= 1
			) {
				this->setState( Hardware::State::READY );
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
				this->m_scheduler.schedule( 1000 * 10, 1, NULL, "solaredge retry", [this]( Scheduler::Task<>& ) -> void {
					this->setState( Hardware::State::SLEEPING );
				} );
				return true;
			}
		} catch( json::exception ex_ ) {
			Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
			this->setState( Hardware::State::FAILED );
		}
		return false;
	};
	
}; // namespace micasa
