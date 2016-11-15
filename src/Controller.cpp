#include <iostream>

#include "Controller.h"

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

	std::string Controller::toString() const {
		return "Controller";
	};

	void Controller::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		
		std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery( "SELECT `id`, `unit`, `name`, `type` FROM `hardware`" );
		for ( auto hardwareIt = hardwareData.begin(); hardwareIt != hardwareData.end(); hardwareIt++ ) {
			Hardware::HardwareType hardwareType = static_cast<Hardware::HardwareType>( atoi( (*hardwareIt)["type"].c_str() ) );
			std::map<std::string, std::string> settings = g_database->getQueryMap( "SELECT `key`, `value` FROM `hardware_settings` WHERE `hardware_id`=%q", (*hardwareIt).at( "id" ).c_str() );

			std::shared_ptr<Hardware> hardware = Hardware::factory( hardwareType, (*hardwareIt)["id"], (*hardwareIt)["unit"], (*hardwareIt)["name"], settings );
			hardware->start();
			this->m_hardware.push_back( hardware );
		}

		g_webServer->addResourceHandler( "hardware", WebServerResource::Method::GET | WebServerResource::Method::HEAD | WebServerResource::Method::POST, this->shared_from_this() );
		g_webServer->addResourceHandler( "devices", WebServerResource::Method::GET | WebServerResource::Method::HEAD, this->shared_from_this() );

		this->_begin();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->_retire();

		g_webServer->removeResourceHandler( "hardware" );
		g_webServer->removeResourceHandler( "devices" );

		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				(*hardwareIt)->stop();
			}
			this->m_hardware.clear();
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( Hardware::HardwareType hardwareType_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) {
#ifdef _DEBUG
		assert( this->isRunning() && "Controller should be running when declaring hardware." );
#endif // _DEBUG
		
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getUnit() == unit_ ) {
				return *hardwareIt;
			}
		}

		long id = g_database->putQuery( "INSERT INTO `hardware` ( `unit`, `type`, `name` ) VALUES ( %Q, %d, %Q )", unit_.c_str(), static_cast<int>( hardwareType_ ), name_.c_str() );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			g_database->putQuery( "INSERT INTO `hardware_settings` ( `hardware_id`, `key`, `value` ) VALUES ( %d, %Q, %Q )", id, settingsIt->first.c_str(), settingsIt->second.c_str() );
		}
		
		std::shared_ptr<Hardware> hardware = Hardware::factory( hardwareType_, std::to_string( id ), unit_, name_, settings_ );
		hardware->start();
		this->m_hardware.push_back( hardware );

		return hardware;
	};

	std::chrono::milliseconds Controller::_work( unsigned long int iteration_ ) {
		return std::chrono::milliseconds( 1000 );
	};

}; // namespace micasa
