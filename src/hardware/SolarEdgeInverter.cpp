#include <ctime>

#include "SolarEdgeInverter.h"
#include "../Database.h"
#include "../Controller.h"
#include "../WebServer.h"

#include "json.hpp"

#include "../device/Level.h"
#include "../device/Counter.h"

namespace micasa {
	
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Network> g_network;
	extern std::shared_ptr<Controller> g_controller;
	
	using namespace nlohmann;
	
	void SolarEdgeInverter::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	}
	
	void SolarEdgeInverter::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	}
	
	const std::chrono::milliseconds SolarEdgeInverter::_work( const unsigned long int& iteration_ ) {
		
		if ( ! this->m_settings.contains( { "api_key", "site_id", "serial" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 60 * 1000 );
		}
		
		auto dates = g_database->getQueryRow(
			"SELECT date('now','-1 day','localtime') AS `startdate`, "
			"time('now','-1 day','localtime') AS `starttime`, "
			"date('now','+1 day','localtime') AS `enddate`, "
			"time('now','+1 day','localtime') AS `endtime` "
		);

		std::stringstream url;
		url << "https://monitoringapi.solaredge.com/equipment/" << this->m_settings["site_id"] << "/" << this->m_settings["serial"] << "/data.json?startTime=" << dates["startdate"] << "%20" << dates["starttime"] << "&endTime=" << dates["enddate"] << "%20" << dates["endtime"] << "&api_key=" << this->m_settings["api_key"];
		
		g_network->connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REPLY ) {
				this->_processHttpReply( connection_, (http_message*)data_ );
			}
		} ) );
		
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	}
	
	void SolarEdgeInverter::_processHttpReply( mg_connection* connection_, const http_message* message_ ) {
		std::string body;
		body.assign( message_->body.p, message_->body.len );
		
		try {
			json data = json::parse( body );
			if (
				data["data"].is_object()
				&& data["data"]["telemetries"].is_array()
				&& ! data["data"]["count"].is_null()
				&& data["data"]["count"].get<int>() > 0
			) {
				json telemetry = *data["data"]["telemetries"].rbegin();
				if ( ! telemetry["totalActivePower"].empty() ) {
					std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, this->getReference() + "(P)", "Power", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
						{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::WATT ) }
					} ) );
					device->updateValue( Device::UpdateSource::HARDWARE, telemetry["totalActivePower"].get<double>() );
				}
				if ( ! telemetry["totalEnergy"].empty() ) {
					std::shared_ptr<Counter> device = std::static_pointer_cast<Counter>( this->_declareDevice( Device::Type::COUNTER, this->getReference() + "(E)", "Energy", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
						{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Counter::Unit::WATTHOUR ) }
					} ) );
					device->updateValue( Device::UpdateSource::HARDWARE, telemetry["totalEnergy"].get<int>() );
				}
				if ( ! telemetry["dcVoltage"].empty() ) {
					std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, this->getReference() + "(DC)", "DC voltage", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
						{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::VOLT ) }
					} ) );
					device->updateValue( Device::UpdateSource::HARDWARE, telemetry["dcVoltage"].get<double>() );
				}
				if ( ! telemetry["temperature"].empty() ) {
					std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, this->getReference() + "(T)", "Temperature", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
						{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::DEGREES ) }
					} ) );
					device->updateValue( Device::UpdateSource::HARDWARE, telemetry["temperature"].get<double>() );
				}
			}
		} catch( ... ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid response." );
		}
		
		connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
	}
	
}; // namespace micasa
