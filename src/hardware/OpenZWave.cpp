#ifdef WITH_OPENZWAVE

#include "OpenZWave.h"

#include "../Logger.h"
#include "../Controller.h"

// OpenZWave includes
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "value_classes/ValueStore.h"
#include "value_classes/Value.h"
#include "value_classes/ValueBool.h"
#include "platform/Log.h"
#include "Defs.h"

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ ) {
	auto target = reinterpret_cast<micasa::OpenZWave*>( handler_ );
	target->handleNotification( notification_ );
}

namespace micasa {

	using namespace ::OpenZWave;
	
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;

	void OpenZWave::start() {
		
		if ( ! this->m_settings.contains( { "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave Version %s.", Manager::getVersionAsString().c_str() );
		
		std::lock_guard<std::mutex> lock( this->m_managerMutex );
		
		// https://github.com/OpenZWave/open-zwave/wiki/Config-Options
		// TODO reliably detect the location of the user folder.
		Options::Create( "./lib/open-zwave/config", "./usr", "" );
		
#ifdef _DEBUG
		Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
		Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
		Options::Get()->AddOptionInt( "DumpTriggerLevel", LogLevel_Error );
		Options::Get()->AddOptionBool( "Logging", true );
#else
		Options::Get()->AddOptionBool( "Logging", false );
#endif // _DEBUG

		Options::Get()->AddOptionBool( "ConsoleOutput", false );
		Options::Get()->AddOptionString( "LogFileName", "openzwave.log", false );
		
		Options::Get()->AddOptionInt( "PollInterval", 60000 ); // 60 seconds
		Options::Get()->AddOptionInt( "DriverMaxAttempts", 3 );
		Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
		Options::Get()->AddOptionBool( "ValidateValueChanges", true );
		Options::Get()->AddOptionBool( "Associate", true );
		Options::Get()->AddOptionBool( "SaveConfiguration", true );
		Options::Get()->AddOptionBool( "AppendLogFile", false );

		Options::Get()->Lock();
		
		if ( NULL != Manager::Create() ) {
			Manager::Get()->AddWatcher( micasa_openzwave_notification_handler, this );
			Manager::Get()->AddDriver( this->m_settings["port"] );
		}

		Hardware::start();
	}
	
	void OpenZWave::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		Manager::Get()->RemoveWatcher( micasa_openzwave_notification_handler, this );
		Manager::Destroy();
		Options::Destroy();

		this->m_settings.commit();
		
		Hardware::stop();
	}
	
	void OpenZWave::handleNotification( const Notification* notification_ ) {
		std::lock_guard<std::mutex> lock( this->m_managerMutex );
		g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );
		
		unsigned int homeId = notification_->GetHomeId();
	
		// http://www.openzwave.com/dev/classOpenZWave_1_1Notification.html#a5fa14ba721a25a4c84e0fbbedd767d54a4432a88465416a63cf4eda11ecf28c24
		switch( notification_->GetType() ) {
			case Notification::Type_ValueAdded: {
				break;
			}
				
			case Notification::Type_ValueRemoved: {
				break;
			}
				
			case Notification::Type_ValueChanged: {
				break;
			}
				
			case Notification::Type_Group: {
				break;
			}
				
			case Notification::Type_NodeAdded: {
				break;
			}
				
			case Notification::Type_NodeRemoved: {
				break;
			}
				
			case Notification::Type_NodeEvent: {
				break;
			}
				
			case Notification::Type_PollingDisabled: {
				break;
			}
				
			case Notification::Type_PollingEnabled: {
				break;
			}
				
			case Notification::Type_DriverReady: {
				// Make sure the hardware didn't change between initializations. This prevents situations where
				// multiple controllers exists and their usb ports have changed. Settings need to be changed to
				// fix this.
				
				if ( this->m_settings.get( "home_id", homeId ) != homeId ) {
				}
				
				
				/*
				if ( this->m_settings[{ "home_id", homeId}] != homeId ) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Driver hardware changed." );
					Manager::Get()->RemoveWatcher( micasa_openzwave_notification_handler, this );
				}
				*/
				break;
			}
				
			case Notification::Type_DriverFailed: {
				g_logger->log( Logger::LogLevel::ERROR, this, "Driver failed to initialize." );
				break;
			}
				
			case Notification::Type_AwakeNodesQueried: {
				break;
			}
				
			case Notification::Type_AllNodesQueried: {
				break;
			}
				
			case Notification::Type_AllNodesQueriedSomeDead: {
				break;
			}
				
			case Notification::Type_DriverReset: {
				break;
			}
				
			case Notification::Type_Notification: {
				break;
			}
				
			case Notification::Type_NodeNaming: {
				break;
			}
				
			case Notification::Type_NodeProtocolInfo: {
				break;
			}
				
			case Notification::Type_NodeQueriesComplete: {
				break;
			}
				
			case Notification::Type_NodeNew: {
				break;
			}
				
			case Notification::Type_SceneEvent: {
				break;
			}
				
			case Notification::Type_CreateButton: {
				break;
			}
				
			case Notification::Type_DeleteButton: {
				break;
			}
				
			case Notification::Type_ButtonOn: {
				break;
			}
				
			case Notification::Type_ButtonOff: {
				break;
			}
				
			case Notification::Type_EssentialNodeQueriesComplete: {
				break;
			}
				
			case Notification::Type_ValueRefreshed: {
				break;
			}
				
			case Notification::Type_DriverRemoved: {
				break;
			}
				
			case Notification::Type_ControllerCommand: {
				break;
			}
				
			case Notification::Type_NodeReset: {
				break;
			}
				
		}
	}
	
}; // namespace micasa

#endif // WITH_OPENZWAVE
