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

	std::chrono::milliseconds Controller::_doWork() {
		return std::chrono::milliseconds( 1000 );
	};

	bool Controller::start() {
		if ( Worker::start() ) {
			g_webServer->addResourceHandler( "hardware", WebServerResource::Method::GET | WebServerResource::Method::HEAD | WebServerResource::Method::POST, this->shared_from_this() );
			g_webServer->addResourceHandler( "devices", WebServerResource::Method::GET | WebServerResource::Method::HEAD, this->shared_from_this() );

			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );

			std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery( "SELECT `id`, `type` FROM `hardware` ORDER BY `type`" );
			for ( auto hardwareIt = hardwareData.begin(); hardwareIt != hardwareData.end(); hardwareIt++ ) {
				Hardware::HardwareType type = static_cast<Hardware::HardwareType>( atoi( (*hardwareIt)["type"].c_str() ) );
				std::map<std::string, std::string> settings = g_database->getQueryMap( "SELECT `key`, `value` FROM `settings` WHERE `hardware_id`=%q", (*hardwareIt).at( "id" ).c_str() );
				std::shared_ptr<Hardware> hardware = Hardware::get( (*hardwareIt)["id"], type, settings );
				if ( NULL != hardware ) {
					hardware->start();
					this->m_hardware.push_back( hardware );
				} else {
					g_logger->log( Logger::LogLevel::ERROR, this, "Hardware #%s has invalid hardware type %s.", (*hardwareIt)["id"].c_str(), (*hardwareIt)["type"].c_str() );
				}
			}
			return true;
		} else {
			return false;
		}
	};

	bool Controller::stop() {
		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				(*hardwareIt)->stop();
			}
			this->m_hardware.clear();
		}

		g_webServer->removeResourceHandler( "hardware" );
		g_webServer->removeResourceHandler( "devices" );

		return Worker::stop();
	};

	std::shared_ptr<Hardware> Controller::declareHardware( Hardware::HardwareType hardwareType, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) {
		
	};

	std::shared_ptr<Hardware> Controller::getHardwareById( std::string id_ ) {
		
	};
	
	std::shared_ptr<Hardware> Controller::getHardwareByUnit( std::string unit_ ) {
		
	};

	
}; // namespace micasa
