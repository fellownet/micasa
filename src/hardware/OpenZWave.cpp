// https://github.com/OpenZWave/open-zwave/wiki/Config-Options
// http://www.openzwave.com/dev/classOpenZWave_1_1Notification.html#a5fa14ba721a25a4c84e0fbbedd767d54a4432a88465416a63cf4eda11ecf28c24

// pipes to store and retrieve the log and config?
// http://stackoverflow.com/questions/2784500/how-to-send-a-simple-string-between-two-programs-using-pipes

// libudev for detecting add- or remove usb device?
// http://www.signal11.us/oss/udev/

#include "OpenZWave.h"
#include "OpenZWaveNode.h"

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
	target->_handleNotification( notification_ );
};

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	OpenZWave::OpenZWave( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		this->m_settings.put<std::string>( HARDWARE_SETTINGS_ALLOWED, "port" );
	};

	void OpenZWave::start() {

		// TODO use libudev-dev http://www.signal11.us/oss/udev/
		// It can also detect removal of device

		if ( ! this->m_settings.contains( { "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave Version %s.", ::OpenZWave::Manager::getVersionAsString().c_str() );

		this->m_managerMutex.lock();

		// TODO reliably detect the location of the user folder.
		::OpenZWave::Options::Create( "./lib/open-zwave/config", "./var", "" );

#ifdef _DEBUG
		::OpenZWave::Options::Get()->AddOptionInt( "SaveLogLevel", ::OpenZWave::LogLevel_Detail );
		::OpenZWave::Options::Get()->AddOptionInt( "QueueLogLevel", ::OpenZWave::LogLevel_Debug );
		::OpenZWave::Options::Get()->AddOptionInt( "DumpTriggerLevel", ::OpenZWave::LogLevel_Error );
		::OpenZWave::Options::Get()->AddOptionBool( "Logging", true );
#else
		::OpenZWave::Options::Get()->AddOptionBool( "Logging", false );
#endif // _DEBUG

		::OpenZWave::Options::Get()->AddOptionBool( "ConsoleOutput", false );
		::OpenZWave::Options::Get()->AddOptionString( "LogFileName", "openzwave.log", false );

		::OpenZWave::Options::Get()->AddOptionInt( "PollInterval", 60000 ); // 60 seconds
		::OpenZWave::Options::Get()->AddOptionInt( "DriverMaxAttempts", 3 );
		::OpenZWave::Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
		::OpenZWave::Options::Get()->AddOptionBool( "ValidateValueChanges", true );
		::OpenZWave::Options::Get()->AddOptionBool( "Associate", true );
		::OpenZWave::Options::Get()->AddOptionBool( "SaveConfiguration", true );
		::OpenZWave::Options::Get()->AddOptionBool( "AppendLogFile", false );

		::OpenZWave::Options::Get()->Lock();

		if ( NULL != ::OpenZWave::Manager::Create() ) {
			::OpenZWave::Manager::Get()->AddWatcher( micasa_openzwave_notification_handler, this );
			::OpenZWave::Manager::Get()->AddDriver( this->m_settings["port"] );
		}

		this->m_managerMutex.unlock();

		Hardware::start();
	};

	void OpenZWave::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		this->m_managerMutex.lock();
		::OpenZWave::Manager::Get()->RemoveWatcher( micasa_openzwave_notification_handler, this );
		::OpenZWave::Manager::Destroy();
		::OpenZWave::Options::Destroy();
		this->m_managerMutex.unlock();

		g_webServer->removeResourceCallback( "openzwave-" + std::to_string( this->m_id ) );

		Hardware::stop();
	};

	const std::string OpenZWave::getLabel() const {
		return this->m_settings.get( "label", "OpenZWave" );
	};

	const std::chrono::milliseconds OpenZWave::_work( const unsigned long int& iteration_ ) {
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	};

	void OpenZWave::_handleNotification( const ::OpenZWave::Notification* notification_ ) {
		this->m_managerMutex.lock();

#ifdef _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );
#endif // _DEBUG

		unsigned int homeId = notification_->GetHomeId();
		unsigned char nodeId = notification_->GetNodeId();

		std::stringstream reference;
		reference << homeId << "/" << (unsigned int)nodeId;
		auto node = std::static_pointer_cast<OpenZWaveNode>( g_controller->getHardware( reference.str() ) );
		if ( node != nullptr ) {

			// This notification belongs to one of the zwave nodes and should be handled directly by this hardware.
			// TODO some notifications for nodes need to be handled by openzwave hardware, such as node dead to
			// initiate a one-per-15 minutes network heal or something.
			node->_handleNotification( notification_ );

		} else {

			// No hardware is available for this node which means that the notification is for the controller (us).
			//g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );
			switch( notification_->GetType() ) {

				case ::OpenZWave::Notification::Type_DriverReady: {
					this->m_homeId = homeId;
					this->m_controllerNodeId = nodeId;
					g_logger->log( Logger::LogLevel::VERBOSE, this, "Driver initialized." );
					break;
				}

				case ::OpenZWave::Notification::Type_DriverFailed: {
					this->_setState( Hardware::State::FAILED );
					g_logger->log( Logger::LogLevel::VERBOSE, this, "Driver failed." );
					break;
				}

				case ::OpenZWave::Notification::Type_AwakeNodesQueried:
				case ::OpenZWave::Notification::Type_AllNodesQueried:
				case ::OpenZWave::Notification::Type_AllNodesQueriedSomeDead: {
					this->_setState( Hardware::State::READY );
					this->_installResourceHandlers();
					break;
				}

				case ::OpenZWave::Notification::Type_ControllerCommand: {
					switch( notification_->GetEvent() ) {
						case ::OpenZWave::Driver::ControllerState_Cancel: {
							//this->m_controllerBusy = false;
							break;
						}
						case ::OpenZWave::Driver::ControllerState_Error: {
							//this->m_controllerBusy = false;
							break;
						}
						case ::OpenZWave::Driver::ControllerState_Completed: {
							//this->m_controllerBusy = false;
							break;
						}
						case ::OpenZWave::Driver::ControllerState_Failed: {
							//this->m_controllerBusy = false;
							break;
						}
					}
					break;
				}

				case ::OpenZWave::Notification::Type_NodeProtocolInfo: {
					if (
						this->m_controllerNodeId > 0
						&& nodeId != this->m_controllerNodeId
						&& homeId == this->m_homeId
					) {
						std::string label = ::OpenZWave::Manager::Get()->GetNodeType( homeId, nodeId );
						g_controller->declareHardware( Hardware::Type::OPEN_ZWAVE_NODE, reference.str(), this->shared_from_this(), {
							{ "label", label },
							{ "home_id", std::to_string( homeId ) }
						}, true /* auto start */ );
					}
				}

				case ::OpenZWave::Notification::Type_NodeNaming: {
					if (
						nodeId == this->m_controllerNodeId
						&& homeId == this->m_homeId
					) {
						std::string manufacturer = ::OpenZWave::Manager::Get()->GetNodeManufacturerName( homeId, nodeId );
						std::string product = ::OpenZWave::Manager::Get()->GetNodeProductName( homeId, nodeId );
						if ( ! manufacturer.empty() ) {
							this->m_settings.put( "label", manufacturer + " " + product );
						}
					}
					break;
				}

				default: {
					break;
				}

			}


		}

		this->m_managerMutex.unlock();


/*
		switch( notification_->GetType() ) {
			case ::OpenZWave::Notification::Type_ValueAdded: {
				break;
			}

			case ::OpenZWave::Notification::Type_ValueRemoved: {
				break;
			}

			case ::OpenZWave::Notification::Type_ValueChanged: {
				break;
			}

			case ::OpenZWave::Notification::Type_Group: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeAdded: {
				// The controller is itself a node but it is not added as separate hardware.
				unsigned int nodeId = notification_->GetNodeId();
				if ( nodeId != this->m_controllerNodeId ) {

					std::map<std::string, std::string> settings;


					std::stringstream label;
					label << ::OpenZWave::Manager::Get()->GetNodeManufacturerName( this->m_homeId, nodeId );
					label << " " << ::OpenZWave::Manager::Get()->GetNodeProductName( this->m_homeId, nodeId );

					g_controller->declareHardware(
						Hardware::Type::OPEN_ZWAVE_NODE,
						this->shared_from_this(),
						reference.str(),
						label.str(),
						settings
					);
				}
				break;
			}

			case ::OpenZWave::Notification::Type_NodeRemoved: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeEvent: {
				break;
			}

			case ::OpenZWave::Notification::Type_PollingDisabled: {
				break;
			}

			case ::OpenZWave::Notification::Type_PollingEnabled: {
				break;
			}

			case ::OpenZWave::Notification::Type_DriverReady: {
				this->m_homeId = notification_->GetHomeId();
				this->m_controllerNodeId = notification_->GetNodeId();
				g_logger->log( Logger::LogLevel::NORMAL, this, "Driver initialized." );
				break;
			}

			case ::OpenZWave::Notification::Type_DriverFailed: {
				g_logger->log( Logger::LogLevel::ERROR, this, "Driver failed to initialize." );
				break;
			}

			case ::OpenZWave::Notification::Type_AwakeNodesQueried: {
				break;
			}

			case ::OpenZWave::Notification::Type_AllNodesQueried: {
				break;
			}

			case ::OpenZWave::Notification::Type_AllNodesQueriedSomeDead: {
				break;
			}

			case ::OpenZWave::Notification::Type_DriverReset: {
				break;
			}

			case ::OpenZWave::Notification::Type_Notification: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeNaming: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeProtocolInfo: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeQueriesComplete: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeNew: {
				break;
			}

			case ::OpenZWave::Notification::Type_SceneEvent: {
				break;
			}

			case ::OpenZWave::Notification::Type_CreateButton: {
				break;
			}

			case ::OpenZWave::Notification::Type_DeleteButton: {
				break;
			}

			case ::OpenZWave::Notification::Type_ButtonOn: {
				break;
			}

			case ::OpenZWave::Notification::Type_ButtonOff: {
				break;
			}

			case ::OpenZWave::Notification::Type_EssentialNodeQueriesComplete: {
				break;
			}

			case ::OpenZWave::Notification::Type_ValueRefreshed: {
				break;
			}

			case ::OpenZWave::Notification::Type_DriverRemoved: {
				break;
			}

			case ::OpenZWave::Notification::Type_ControllerCommand: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeReset: {
				break;
			}

		}
*/
	};

	void OpenZWave::_installResourceHandlers() const {
		// Add resource handlers for network heal.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"openzwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/heal",
			WebServer::Method::PUT,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( this->m_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					if ( this->getState() == Hardware::State::READY ) {
						::OpenZWave::Manager::Get()->HealNetwork( this->m_homeId, true );
						output_["result"] = "OK";
						g_logger->log( Logger::LogLevel::NORMAL, this, "Network heal initiated." );
					} else {
						output_["result"] = "ERROR";
						output_["message"] = "Controller not ready.";
						code_ = 423; // Locked (WebDAV; RFC 4918)
					}
					this->m_managerMutex.unlock();
				} else {
					output_["result"] = "ERROR";
					output_["message"] = "Controller busy.";
					code_ = 423; // Locked (WebDAV; RFC 4918)
				}
			} )
		} ) ) );

		// Add resource handler for inclusion mode.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"openzwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/include",
			WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				// TODO also accept secure inclusion mode
				// TODO cancel inclusion mode after xx minutes? openzwave doesn't cancel
				if ( this->m_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					if ( method_ == WebServer::Method::PUT ) {
						if ( this->getState() == Hardware::State::READY ) {
							if ( ::OpenZWave::Manager::Get()->AddNode( this->m_homeId, false ) ) {
								output_["result"] = "OK";
								g_logger->log( Logger::LogLevel::NORMAL, this, "Inclusion mode activated." );
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to activate inclusion mode.";
								code_ = 500; // Internal Server Error
								g_logger->log( Logger::LogLevel::ERROR, this, "Unable to activate inclusion mode." );
							}
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Controller not ready.";
							code_ = 423; // Locked (WebDAV; RFC 4918)
						}
					} else if ( method_ == WebServer::Method::DELETE ) {
						if ( this->getState() == Hardware::State::READY ) {
							if ( ::OpenZWave::Manager::Get()->CancelControllerCommand( this->m_homeId ) ) {
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to deactivate inclusion mode.";
								code_ = 500; // Internal Server Error
								g_logger->log( Logger::LogLevel::ERROR, this, "Unable to deactivate inclusion mode." );
							}
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Controller not ready.";
							code_ = 409; // Conflict
						}
					}
					this->m_managerMutex.unlock();
				} else {
					output_["result"] = "ERROR";
					output_["message"] = "Controller busy.";
					code_ = 423; // Locked (WebDAV; RFC 4918)
				}
			} )
		} ) ) );

		// Add resource handler for exclusion mode.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"openzwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/exclude",
			WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				// TODO cancel exclusion mode after xx minutes? openzwave doesn't cancel
				if ( this->m_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					if ( method_ == WebServer::Method::PUT ) {
						if ( this->getState() == Hardware::State::READY ) {
							if ( ::OpenZWave::Manager::Get()->RemoveNode( this->m_homeId ) ) {
								output_["result"] = "OK";
								g_logger->log( Logger::LogLevel::NORMAL, this, "Exclusion mode activated." );
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to activate exclusion mode.";
								code_ = 500; // Internal Server Error
								g_logger->log( Logger::LogLevel::ERROR, this, "Unable to activate exclusion mode." );
							}
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Controller not ready.";
							code_ = 423; // Locked (WebDAV; RFC 4918)
						}
					} else if ( method_ == WebServer::Method::DELETE ) {
						if ( this->getState() == Hardware::State::READY ) {
							if ( ::OpenZWave::Manager::Get()->CancelControllerCommand( this->m_homeId ) ) {
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to deactivate exclusion mode.";
								code_ = 500; // Internal Server Error
								g_logger->log( Logger::LogLevel::ERROR, this, "Unable to deactivate exclusion mode." );
							}
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Controller not ready.";
							code_ = 409; // Conflict
						}
					}
					this->m_managerMutex.unlock();
				} else {
					output_["result"] = "ERROR";
					output_["message"] = "Controller busy.";
					code_ = 423; // Locked (WebDAV; RFC 4918)
				}
			} )
		} ) ) );
	};

}; // namespace micasa
