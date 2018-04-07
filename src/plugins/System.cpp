#include "System.h"

#include "../Logger.h"
#include "../User.h"
#include "../Utils.h"
#include "../Network.h"
#include "../Scheduler.h"
#include "../Database.h"
#include "../device/Level.h"
#include "../device/Counter.h"

namespace micasa {

	extern std::unique_ptr<Database> g_database;

	const char* System::label = "System";

	void System::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();
		this->setState( Plugin::State::READY );
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_1MIN, SCHEDULER_INTERVAL_1MIN, SCHEDULER_REPEAT_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			this->declareDevice<Level>( "network_connections", "Network Connections", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::GENERIC ) },
				{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::GENERIC ) }
			} )->updateValue( Device::UpdateSource::PLUGIN, Network::get().m_connections.size() );

			this->declareDevice<Level>( "pending_tasks", "Pending Tasks", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::GENERIC ) },
				{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::GENERIC ) }
			} )->updateValue( Device::UpdateSource::PLUGIN, Scheduler::ThreadPool::get().m_tasks.size() );

			this->declareDevice<Counter>( "database_queries", "Database Queries", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Counter::resolveTextSubType( Counter::SubType::GENERIC ) },
				{ DEVICE_SETTING_DEFAULT_UNIT,           Counter::resolveTextUnit( Counter::Unit::GENERIC ) }
			} )->updateValue( Device::UpdateSource::PLUGIN, g_database->m_queries );
		} );
	};

	void System::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		Plugin::stop();
	};

	bool System::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) {
		if ( owned_ ) {
		}
		return true;
	};

}; // namespace micasa
