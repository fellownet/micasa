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
		
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Worker::stop();

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
		
		return hardware;
	};
	
	void Controller::addTask( const std::shared_ptr<Task> task_ ) {
		std::unique_lock<std::mutex> lock( this->m_taskQueueMutex );
		
		// The task is inserted in the list in order of scheduled time. This way the worker method only needs to
		// investigate the front of the queue.
		auto taskIt = this->m_taskQueue.begin();
		while(
			taskIt != this->m_taskQueue.end()
			&& task_->scheduled > (*taskIt)->scheduled
		) {
			taskIt++;
		}
		this->m_taskQueue.insert( taskIt, task_ );
		lock.unlock();
		
		// Immediately wake up the worker to have it start processing scheduled items.
		this->wakeUp();
	};
	
	std::chrono::milliseconds Controller::_work( const unsigned long int iteration_ ) {
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
		
		// Investigate the front of the queue for tasks that are due. If the next task is not due we're done due to
		// the fact that addTask keeps the list sorted on scheduled time.
		auto taskQueueIt = this->m_taskQueue.begin();
		auto now = std::chrono::system_clock::now();
		while(
			taskQueueIt != this->m_taskQueue.end()
			&& (*taskQueueIt)->scheduled <= now + std::chrono::milliseconds( 5 )
		) {
			std::shared_ptr<Task> task = (*taskQueueIt);
			
			g_logger->log( Logger::LogLevel::VERBOSE, this, "TASK" );
			
			taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
		}
		
		// If there's nothing in the queue anymore we can wait a 'long' time before investigating the queue again. This
		// is because the addTask method will wake us up if there's work to do anyway.
		auto wait = std::chrono::milliseconds( 15000 );
		if ( taskQueueIt != this->m_taskQueue.end() ) {
			auto delay = (*taskQueueIt)->scheduled - now;
			wait = std::chrono::duration_cast<std::chrono::milliseconds>( delay );
		}
		
		return std::chrono::milliseconds( wait );
	};
	
}; // namespace micasa
