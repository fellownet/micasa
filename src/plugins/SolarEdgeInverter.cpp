#include <ctime>
#include <sstream>

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

	extern std::unique_ptr<Database> g_database;

	using namespace nlohmann;

	const char* SolarEdgeInverter::label = "SolarEdge Inverter";

	void SolarEdgeInverter::start() {
#ifdef _DEBUG
		assert( this->m_settings->contains( { "api_key", "site_id", "serial" } ) && "SolarEdgeInverter should be declared with mandatory settings." );
#endif // _DEBUG
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();

		this->m_scheduler.schedule( 0, SCHEDULER_INTERVAL_5MIN, SCHEDULER_REPEAT_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			if ( ! this->m_settings->contains( { "api_key", "site_id", "serial" } ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Plugin::State::FAILED );
				return;
			}

			auto dates = g_database->getQueryRow(
				"SELECT date('now','-1 day','localtime') AS `startdate`, "
				"time('now','-1 day','localtime') AS `starttime`, "
				"date('now','+1 day','localtime') AS `enddate`, "
				"time('now','+1 day','localtime') AS `endtime` "
			);

			if ( this->m_connection != nullptr ) {
				this->m_connection->terminate();
			}

			std::stringstream url;
			url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings->get( "site_id" ) << "/" << this->m_settings->get( "serial" ) << "/data.json?startTime=" << dates["startdate"] << "%20" << dates["starttime"] << "&endTime=" << dates["enddate"] << "%20" << dates["endtime"] << "&api_key=" << this->m_settings->get( "api_key" );
			this->m_connection = Network::connect( url.str(), [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) {
				switch( event_ ) {
					case Network::Connection::Event::CONNECT: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );
						break;
					}
					case Network::Connection::Event::FAILURE: {
						Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
						this->setState( Plugin::State::FAILED );
						break;
					}
					case Network::Connection::Event::HTTP: {
						this->_process( connection_->getBody() );
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

	void SolarEdgeInverter::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		if ( this->m_connection != nullptr ) {
			this->m_connection->terminate();
		}
		Plugin::stop();
	};

	std::string SolarEdgeInverter::getLabel() const {
		return this->m_settings->get( "label", std::string( SolarEdgeInverter::label ) );
	};

	void SolarEdgeInverter::_process( const std::string& data_ ) {
		try {
			json data = json::parse( data_ );
			if (
				data["data"].is_object()
				&& data["data"]["telemetries"].is_array()
				&& ! data["data"]["count"].is_null()
				&& data["data"]["count"].get<int>() > 0
			) {
				this->setState( Plugin::State::READY );
				json telemetry = *data["data"]["telemetries"].rbegin();
				if ( ! telemetry["totalActivePower"].empty() ) {
					auto device = this->declareDevice<Level>( this->getReference() + "(P)", "Power", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::POWER ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::WATT ) }
					} );
					device->updateValue( Device::UpdateSource::PLUGIN, telemetry["totalActivePower"].get<double>() );
				}
				if ( ! telemetry["totalEnergy"].empty() ) {
					auto device = this->declareDevice<Counter>( this->getReference() + "(E)", "Energy", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Counter::resolveTextSubType( Counter::SubType::ENERGY ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Counter::resolveTextUnit( Counter::Unit::KILOWATTHOUR ) }
					} );
					device->updateValue( Device::UpdateSource::PLUGIN, telemetry["totalEnergy"].get<int>() / 1000. );
				}
				if ( ! telemetry["dcVoltage"].empty() ) {
					auto device = this->declareDevice<Level>( this->getReference() + "(DC)", "DC voltage", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::ELECTRICITY ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::VOLT ) }
					} );
					device->updateValue( Device::UpdateSource::PLUGIN, telemetry["dcVoltage"].get<double>() );
				}
				if ( ! telemetry["temperature"].empty() ) {
					auto device = this->declareDevice<Level>( this->getReference() + "(T)", "Temperature", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::TEMPERATURE ) },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::CELSIUS ) }
					} );
					device->updateValue( Device::UpdateSource::PLUGIN, telemetry["temperature"].get<double>() );
				}
				this->m_scheduler.schedule( 1000 * 10, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) -> void {
					this->setState( Plugin::State::SLEEPING );
				} );
			}
		} catch( json::exception ex_ ) {
			Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
			this->setState( Plugin::State::FAILED );
		}
	};

}; // namespace micasa
