#include <ctime>

#include "SolarEdge.h"
#include "../Database.h"

#include "json.hpp"

#include "../device/Level.h"
#include "../device/Counter.h"

extern "C" {
	
	void solaredge_mg_handler( mg_connection* connection_, int event_, void* data_ ) {
		std::pair<micasa::SolarEdge*,std::string>* userData = reinterpret_cast<std::pair<micasa::SolarEdge*,std::string>*>( connection_->user_data );
		if ( event_ == MG_EV_HTTP_REPLY ) {
			userData->first->_processHttpReply( connection_, (http_message*)data_ );
		} else if ( event_ == MG_EV_CONNECT ) {
			mg_set_timer( connection_, mg_time() + 5 );
		} else if ( event_ == MG_EV_TIMER ) {
			userData->first->_processHttpReply( connection_, NULL );
		}
	}
	
} // extern "C"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Database> g_database;
	
	using namespace nlohmann;
	
	std::string SolarEdge::toString() const {
		return this->m_name;
	}

	void SolarEdge::start() {
		this->_begin();
		Hardware::start();
	}
	
	void SolarEdge::stop() {
		this->m_busy = 0;
		this->_retire();
		Hardware::stop();
	}
	
	std::chrono::milliseconds SolarEdge::_work( const unsigned long int iteration_ ) {
		
		// Make sure all the required settings are present.
		if ( ! this->m_settings.contains( { "api_key", "site_id" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		int wait = 1000 * 60 * 5;
		
		mg_mgr manager;
		mg_mgr_init( &manager, NULL );
		mg_connection* connection;
		
		std::lock_guard<std::mutex> lock( this->m_invertersMutex );
		
		if ( this->m_inverters.empty() ) {
			// If the list of inverters is empty we should refresh it first. This list contains the inverter
			// serials that  are needed to fetch the actual energy/power data.
			url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings["site_id"] << "/list?api_key=" << this->m_settings["api_key"];
#ifdef _DEBUG
			g_logger->logr( Logger::LogLevel::DEBUG, this, "Fetching from %s.", url.str().c_str() );
#endif // _DEBUG
			connection = mg_connect_http( &manager, solaredge_mg_handler, url.str().c_str(), "Accept: application/json\r\n", NULL );
			if ( NULL != connection ) {
				connection->user_data = new std::pair<SolarEdge*,std::string>( this, "" );
				mg_set_timer( connection, mg_time() + 5 );
				this->m_busy = 1;
				while( this->m_busy > 0 ) {
					mg_mgr_poll( &manager, 1000 );
				}
				if ( this->m_inverters.empty() ) {
					// The list is still empty, so either there are no inverters on this site or fetching them
					// failed. Try again in 5 minutes.
					g_logger->log( Logger::LogLevel::WARNING, this, "No inverters reported." );
				} else {
					// We've got a list of inverters, immediately fetch the data for each one of them.
					wait = 0;
				}
			} else {
				g_logger->log( Logger::LogLevel::ERROR, this, "Unable to connect." );
			}
		} else {
			// We're only interrested in the last reported value by the inverter, so adding a full day to the begin
			// and end time should cover all timezones :)
			auto dates = g_database->getQueryRow(
				"SELECT date('now','-1 day','localtime') AS `startdate`, "
				"time('now','-1 day','localtime') AS `starttime`, "
				"date('now','+1 day','localtime') AS `enddate`, "
				"time('now','+1 day','localtime') AS `endtime` "
			);
			for ( auto inverterIt = this->m_inverters.begin(); inverterIt != this->m_inverters.end(); inverterIt++ ) {
				url.str( "" );
				url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings["site_id"] << "/" << *inverterIt << "/data.json?startTime=" << dates["startdate"] << "%20" << dates["starttime"] << "&endTime=" << dates["enddate"] << "%20" << dates["endtime"] << "&api_key=" << this->m_settings["api_key"];
#ifdef _DEBUG
				g_logger->logr( Logger::LogLevel::DEBUG, this, "Fetching from %s.", url.str().c_str() );
#endif // _DEBUG
				connection = mg_connect_http( &manager, solaredge_mg_handler, url.str().c_str(), "Accept: application/json\r\n", NULL );
				if ( NULL != connection ) {
					connection->user_data = new std::pair<SolarEdge*,std::string>( this, *inverterIt );
					mg_set_timer( connection, mg_time() + 5 );
					this->m_busy++;
				} else {
					g_logger->log( Logger::LogLevel::ERROR, this, "Unable to connect." );
				}
			}
			while( this->m_busy > 0 ) {
				mg_mgr_poll( &manager, 1000 );
			}
		}
		
		mg_mgr_free( &manager );
		
		return std::chrono::milliseconds( wait );
	}

	void SolarEdge::_processHttpReply( mg_connection* connection_, const http_message* message_ ) {
		const std::pair<micasa::SolarEdge*,std::string>* userData = reinterpret_cast<std::pair<micasa::SolarEdge*,std::string>*>( connection_->user_data );
		
		if ( message_ == NULL ) {
			g_logger->log( Logger::LogLevel::WARNING, this, "Timeout when connecting." );
		} else {
			std::string body;
			body.assign( message_->body.p, message_->body.len );

			try {
				json data = json::parse( body );

				// Check for the inverter list reply.
				if (
					! data["reporters"].empty()
					&& ! data["reporters"]["count"].empty()
					&& data["reporters"]["count"] >= 1
				) {
					json list = data["reporters"]["list"];
					for ( auto inverterIt = list.begin(); inverterIt != list.end(); inverterIt++ ) {
						std::stringstream name;
						name << (*inverterIt)["manufacturer"].get<std::string>() << " " << (*inverterIt)["model"].get<std::string>();
						name << " (" << (*inverterIt)["serialNumber"].get<std::string>() << ")";
						this->m_inverters.push_front( (*inverterIt)["serialNumber"].get<std::string>() );
						g_logger->logr( Logger::LogLevel::VERBOSE, this, "Inverter %s reported.", name.str().c_str() );
					}
				}
				
				// Check for the telemetries reply.
				if (
					! data["data"].empty()
					&& ! data["data"]["count"].empty()
				) {
					if ( data["data"]["count"] >= 1 ) {
						json telemetry = *data["data"]["telemetries"].rbegin();
#ifdef _DEBUG
						g_logger->logr( Logger::LogLevel::DEBUG, this, "Received %s.", telemetry.dump().c_str() );
#endif // _DEBUG

						// TODO compare the datetime of the telemetry (date property) to last received date (from settings) and
						// see if it's new. Only declareDevices when it's new.
						if ( ! telemetry["totalActivePower"].empty() ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, userData->second + "(P)", "Power of inverter " + userData->second, { } ) );
							device->updateValue( Device::UpdateSource::HARDWARE, telemetry["totalActivePower"].get<float>() );
						}
						if ( ! telemetry["totalEnergy"].empty() ) {
							std::shared_ptr<Counter> device = std::static_pointer_cast<Counter>( this->_declareDevice( Device::DeviceType::COUNTER, userData->second + "(E)", "Energy of inverter " + userData->second, { } ) );
							device->updateValue( Device::UpdateSource::HARDWARE, telemetry["totalEnergy"].get<float>() );
						}
						if ( ! telemetry["dcVoltage"].empty() ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, userData->second + "(DC)", "DC voltage of inverter " + userData->second, { } ) );
							device->updateValue( Device::UpdateSource::HARDWARE, telemetry["dcVoltage"].get<float>() );
						}
						if ( ! telemetry["temperature"].empty() ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::DeviceType::LEVEL, userData->second + "(T)", "Temperature of inverter " + userData->second, { } ) );
							device->updateValue( Device::UpdateSource::HARDWARE, telemetry["temperature"].get<float>() );
						}
					} else {
						g_logger->logr( Logger::LogLevel::NORMAL, this, "No telemetries for inverter %s.", userData->second.c_str() );
					}
				}
				
			} catch( ... ) {
				// Unfortunately the response from SolarEdge could not be parsed.
				g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
			}
		}
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
		this->m_busy--;
		delete( userData );
	}
	
}; // namespace micasa
