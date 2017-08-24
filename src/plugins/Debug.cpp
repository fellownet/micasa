#include "Debug.h"

#include "../Logger.h"
#include "../User.h"
#include "../Utils.h"
#include "../Network.h"
#include "../device/Counter.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Switch.h"

namespace micasa {

	using namespace std::chrono;

	extern bool g_shutdown;

	const char* Debug::label = "Debug";

	void Debug::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();

		this->declareDevice<Switch>( "shutdown", "Shutdown", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::PLUGIN, Switch::Option::IDLE );

		this->m_scheduler.schedule( 0, SCHEDULER_INTERVAL_10SEC, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			static auto start = system_clock::now();
			this->declareDevice<Level>( "uptime", "Uptime", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) },
				{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::SECONDS ) }
			} )->updateValue( Device::UpdateSource::PLUGIN, duration_cast<seconds>( system_clock::now() - start ).count() );
			this->declareDevice<Level>( "tasks", "Scheduled Tasks", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) }
			} )->updateValue( Device::UpdateSource::PLUGIN, Scheduler::count() );
			this->declareDevice<Level>( "connections", "Connections", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN ) }
			} )->updateValue( Device::UpdateSource::PLUGIN, Network::count() );
		} );

		this->setState( Plugin::State::READY );
	};

	void Debug::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		Plugin::stop();
	};

	bool Debug::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) {
		if ( owned_ ) {
			if ( device_->getType() == Device::Type::SWITCH ) {
				std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
				if ( device->getValueOption() == Switch::Option::ACTIVATE ) {
					std::string type = device->getReference();
					if ( type == "shutdown" ) {
						micasa::g_shutdown = true;
					}
				}
			}
		}
		return true;
	};

}; // namespace micasa
