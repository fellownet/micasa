#include <iostream>

#include "Controller.h"
#include "Database.h"

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

namespace micasa {

	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	Controller::Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before global Controller instance." );
		assert( g_database && "Global Database instance should be created before global Controller instance." );
		assert( g_logger && "Global Logger instance should be created before global Controller instance." );
#endif // _DEBUG
	};

	Controller::~Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after global Controller instance." );
		assert( g_database && "Global Database instance should be destroyed after global Controller instance." );
		assert( g_logger && "Global Logger instance should be destroyed after global Controller instance." );
#endif // _DEBUG
	};

	void Controller::start() {
#ifdef _DEBUG
		assert( g_webServer->isRunning() && "WebServer should be running before Controller is started." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );

		std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery(
			"SELECT `id`, `reference`, `name`, `type` "
			"FROM `hardware`"
			"WHERE `enabled`=1"
		);
		for ( auto hardwareIt = hardwareData.begin(); hardwareIt != hardwareData.end(); hardwareIt++ ) {
			Hardware::HardwareType hardwareType = static_cast<Hardware::HardwareType>( atoi( (*hardwareIt)["type"].c_str() ) );
			std::shared_ptr<Hardware> hardware = Hardware::_factory( hardwareType, (*hardwareIt)["id"], (*hardwareIt)["reference"], (*hardwareIt)["name"] );
			hardware->start();
			this->m_hardware.push_back( hardware );
		}
		
		/*
		g_webServer->addResource( {
			"api/hardware",
			WebServer::ResourceMethod::GET | WebServer::ResourceMethod::HEAD | WebServer::ResourceMethod::POST,
			"Retrieve a list of available hardware.",
			this->shared_from_this()
		} );
		g_webServer->addResource( {
			"api/devices",
			WebServer::ResourceMethod::GET | WebServer::ResourceMethod::HEAD,
			"Retrieve a list of available devices.",
			this->shared_from_this()
		} );
		*/
		
		this->_begin();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->_retire();

		//g_webServer->removeResourceAt( "api/hardware" );
		//g_webServer->removeResourceAt( "api/devices" );

		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				(*hardwareIt)->stop();
			}
			this->m_hardware.clear();
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::HardwareType hardwareType_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ ) {
		return this->declareHardware( hardwareType_, std::shared_ptr<Hardware>(), reference_, name_, settings_ );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::HardwareType hardwareType_, const std::shared_ptr<Hardware> parent_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ ) {
#ifdef _DEBUG
		assert( this->isRunning() && "Controller should be running when declaring hardware." );
#endif // _DEBUG
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				return *hardwareIt;
			}
		}

		long id;
		if ( parent_ ) {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `hardware_id`, `reference`, `type`, `name` ) "
				"VALUES ( %q, %Q, %d, %Q )"
				, parent_->getId().c_str(), reference_.c_str(), static_cast<int>( hardwareType_ ), name_.c_str()
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `reference`, `type`, `name` ) "
				"VALUES ( %Q, %d, %Q )"
				, reference_.c_str(), static_cast<int>( hardwareType_ ), name_.c_str()
			);
		}
		
		std::shared_ptr<Hardware> hardware = Hardware::_factory( hardwareType_, std::to_string( id ), reference_, name_ );
		
		Settings& settings = hardware->getSettings();
		settings.insert( settings_ );
		settings.commit( *hardware );
		
		hardware->start();
		this->m_hardware.push_back( hardware );
		
		//g_webServer->touchResourceAt( "api/hardware" );
		
		return hardware;
	};
	
	std::chrono::milliseconds Controller::_work( const unsigned long int iteration_ ) {
		return std::chrono::milliseconds( 1000 );
	};
	
	/*
	void Controller::handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) {
		if ( resource_.uri == "api/hardware" ) {
			// TODO this makes everything a string :/ use proper types for int and float
			output_ = g_database->getQuery(
				"SELECT `id`, `type`, `name` "
				"FROM `hardware`"
			);
		} else if ( resource_.uri == "api/devices" ) {
			output_ = g_database->getQuery(
				"SELECT `id`, `hardware_id`, `type`, `name` "
				"FROM `devices`"
			);
		}
	};
	*/

}; // namespace micasa
