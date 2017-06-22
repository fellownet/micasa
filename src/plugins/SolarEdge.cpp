#include <ctime>
#include <sstream>

#include "SolarEdge.h"

#include "../Logger.h"
#include "../Network.h"
#include "../Controller.h"

#include "json.hpp"

namespace micasa {

	using namespace nlohmann;

	const char* SolarEdge::label = "SolarEdge API";

	extern std::unique_ptr<Controller> g_controller;

	void SolarEdge::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();

		this->m_task = this->m_scheduler.schedule( 0, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			if ( ! this->m_settings->contains( { "api_key", "site_id" } ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Plugin::State::FAILED );
				return;
			}

			if ( this->m_connection != nullptr ) {
				this->m_connection->terminate();
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
						this->setState( Plugin::State::FAILED );
						this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, this->m_task );
						break;
					}
					case Network::Connection::Event::HTTP: {
						if ( ! this->_process( connection_->getBody() ) ) {
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
			this->m_connection->terminate();
		}
		Plugin::stop();
	};

	json SolarEdge::getJson() const {
		json result = Plugin::getJson();
		result["api_key"] = this->m_settings->get( "api_key", "" );
		result["site_id"] = this->m_settings->get( "site_id", "" );
		return result;
	};

	json SolarEdge::getSettingsJson() const {
		json result = Plugin::getSettingsJson();
		for ( auto &&setting : SolarEdge::getEmptySettingsJson( true ) ) {
			result.push_back( setting );
		}
		return result;
	};

	json SolarEdge::getEmptySettingsJson( bool advanced_ ) {
		json result = json::array();
		result += {
			{ "name", "api_key" },
			{ "label", "API Key" },
			{ "type", "string" },
			{ "mandatory", true },
			{ "class", advanced_ ? "advanced" : "normal" },
			{ "sort", 98 }
		};
		result += {
			{ "name", "site_id" },
			{ "label", "Site ID" },
			{ "type", "string" },
			{ "mandatory", true },
			{ "class", advanced_ ? "advanced" : "normal" },
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
				this->setState( Plugin::State::READY );
				json list = data["reporters"]["list"];
				for ( auto inverterIt = list.begin(); inverterIt != list.end(); inverterIt++ ) {
					std::stringstream label;
					label << (*inverterIt)["manufacturer"].get<std::string>() << " " << (*inverterIt)["model"].get<std::string>();
					g_controller->declarePlugin(
						Plugin::Type::SOLAREDGE_INVERTER,
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
				this->m_scheduler.schedule( 1000 * 10, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) -> void {
					this->setState( Plugin::State::SLEEPING );
				} );
				return true;
			}
		} catch( json::exception ex_ ) {
			Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
			this->setState( Plugin::State::FAILED );
		}
		return false;
	};

}; // namespace micasa
