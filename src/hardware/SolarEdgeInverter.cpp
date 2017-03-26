#include <ctime>

#include "SolarEdgeInverter.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Network.h"
#include "../device/Level.h"
#include "../device/Counter.h"

#include "json.hpp"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {
	
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Database> g_database;
	
	using namespace nlohmann;
	
	void SolarEdgeInverter::start() {
#ifdef _DEBUG
		assert( this->m_settings->contains( { "api_key", "site_id", "serial" } ) && "SolarEdgeInverter should be declared with mandatory settings." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
		
		this->m_scheduler.schedule( 0, SCHEDULER_INTERVAL_5MIN, SCHEDULER_INFINITE, this, [this]( Scheduler::Task<>& ) {
			if ( ! this->m_settings->contains( { "api_key", "site_id", "serial" } ) ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Hardware::State::FAILED );
				return;
			}

			auto dates = g_database->getQueryRow(
				"SELECT date('now','-1 day','localtime') AS `startdate`, "
				"time('now','-1 day','localtime') AS `starttime`, "
				"date('now','+1 day','localtime') AS `enddate`, "
				"time('now','+1 day','localtime') AS `endtime` "
			);

			std::stringstream url;
			url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings->get( "site_id" ) << "/" << this->m_settings->get( "serial" ) << "/data.json?startTime=" << dates["startdate"] << "%20" << dates["starttime"] << "&endTime=" << dates["enddate"] << "%20" << dates["endtime"] << "&api_key=" << this->m_settings->get( "api_key" );
			
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
	
	void SolarEdgeInverter::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		Hardware::stop();
	};

	std::string SolarEdgeInverter::getLabel() const {
		return this->m_settings->get( "label", std::string( SolarEdgeInverter::label ) );
	};

	void SolarEdgeInverter::_process( const std::string& body_ ) {
		try {
			json data = json::parse( body_ );
			if (
				data["data"].is_object()
				&& data["data"]["telemetries"].is_array()
				&& ! data["data"]["count"].is_null()
				&& data["data"]["count"].get<int>() > 0
			) {
				json telemetry = *data["data"]["telemetries"].rbegin();

				Device::UpdateSource source = Device::UpdateSource::HARDWARE;
				if ( this->m_first ) {
					source |= Device::UpdateSource::INIT;
					this->m_first = false;
				}
				
				if ( ! telemetry["totalActivePower"].empty() ) {
					auto device = this->declareDevice<Level>( this->getReference() + "(P)", "Power", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::POWER ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::WATT ) }
					} );
					device->updateValue( source, telemetry["totalActivePower"].get<double>() );
				}
				if ( ! telemetry["totalEnergy"].empty() ) {
					auto device = this->declareDevice<Counter>( this->getReference() + "(E)", "Energy", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Counter::resolveSubType( Counter::SubType::ENERGY ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Counter::resolveUnit( Counter::Unit::KILOWATTHOUR ) }
					} );
					device->updateValue( source, telemetry["totalEnergy"].get<int>() );
				}
				if ( ! telemetry["dcVoltage"].empty() ) {
					auto device = this->declareDevice<Level>( this->getReference() + "(DC)", "DC voltage", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::ELECTRICITY ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::VOLT ) }
					} );
					device->updateValue( source, telemetry["dcVoltage"].get<double>() );
				}
				if ( ! telemetry["temperature"].empty() ) {
					auto device = this->declareDevice<Level>( this->getReference() + "(T)", "Temperature", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::TEMPERATURE ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::CELSIUS ) }
					} );
					device->updateValue( source, telemetry["temperature"].get<double>() );
				}

				this->setState( Hardware::State::READY );
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
			this->setState( Hardware::State::FAILED );
		}
	};
	
}; // namespace micasa
