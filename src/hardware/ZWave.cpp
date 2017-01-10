// https://github.com/OpenZWave/open-zwave/wiki/Config-Options
// http://www.openzwave.com/dev/classOpenZWave_1_1Notification.html#a5fa14ba721a25a4c84e0fbbedd767d54a4432a88465416a63cf4eda11ecf28c24

// pipes to store and retrieve the log and config?
// http://stackoverflow.com/questions/2784500/how-to-send-a-simple-string-between-two-programs-using-pipes

#include "ZWave.h"
#include "ZWaveNode.h"

#include "../Logger.h"
#include "../Controller.h"
#include "../Utils.h"

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
	auto target = reinterpret_cast<micasa::ZWave*>( handler_ );
	target->_handleNotification( notification_ );
};

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	std::timed_mutex ZWave::g_managerMutex;
	unsigned int ZWave::g_managerWatchers = 0;

	ZWave::ZWave( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		// The settings for ZWave need to be entered before the hardware is started. Therefore the
		// resource handler needs to be installed upon construction time. The resource will be destroyed by
		// the controller which uses the same identifier for specific hardware resources.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ), 99, // just prior to the generic callback handler
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				auto settings = extractSettingsFromJson( input_ );
				try {
					this->m_settings->put( "port", settings.at( "port" ) );
				} catch( std::out_of_range exception_ ) { };
				if ( this->m_settings->isDirty() ) {
					this->m_settings->commit( *this );
					this->m_needsRestart = true;
				}
			} )
		} ) ) );
	};

	void ZWave::start() {
		if ( ! this->m_settings->contains( { "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave Version %s.", Manager::getVersionAsString().c_str() );

		std::unique_lock<std::timed_mutex> lock( ZWave::g_managerMutex );
		if (
			NULL == Manager::Get()
			|| 0 == g_managerWatchers
		) {
			Options::Create( "./lib/open-zwave/config", "./var", "" );
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
			
			Manager::Create();
		}

		// There should be a valid manager right now, fail if there isn't or if we're unable to use it.
		if (
			NULL == Manager::Get()
			|| ! Manager::Get()->AddWatcher( micasa_openzwave_notification_handler, this )
			|| ! Manager::Get()->AddDriver( this->m_settings->get( "port" ) )
		) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Unable to initialize OpenZWave manager." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		this->m_port = this->m_settings->get( "port" );
		this->m_homeId = this->m_settings->get<unsigned int>( "home_id", 0 );

		g_managerWatchers++;
		lock.unlock();
		
		this->_installResourceHandlers();

#ifdef _WITH_LIBUDEV
		// If libudev is available we can detect if a device is added or removed from the system.
		g_controller->addSerialPortCallback( "openzwave_" + this->m_id, [this]( const std::string& serialPort_, const std::string& action_ ) {
			std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex );

			// If the serial port we're currently using was removed, set the device state to disabled.
			if (
				serialPort_ == this->m_port
				&& action_ == "remove"
			) {
				Manager::Get()->RemoveDriver( serialPort_ );
				this->m_port.clear();
				
				this->setState( Hardware::State::DISCONNECTED );
				auto children = g_controller->getChildrenOfHardware( *this );
				for ( auto childrenIt = children.begin(); childrenIt != children.end(); childrenIt++ ) {
					(*childrenIt)->setState( Hardware::State::DISCONNECTED );
				}

				g_logger->log( Logger::LogLevel::WARNING, this, "Disconnected." );
			}

			// If a serial port was added (not necessarely ours) and we're currently disabled, see if it
			// is the device we should use.
			// TODO make this more rebust and detect what kind of device was inserted before feeding it
			// to the OpenZWave library?
			if (
				action_ == "add"
				&& (
					this->getState() == Hardware::State::DISCONNECTED
					|| this->getState() == Hardware::State::FAILED
				)
				&& Manager::Get()->AddDriver( serialPort_ )
			) {
				this->m_port = serialPort_;
			
				this->setState( Hardware::State::INIT );
				auto children = g_controller->getChildrenOfHardware( *this );
				for ( auto childrenIt = children.begin(); childrenIt != children.end(); childrenIt++ ) {
					(*childrenIt)->setState( Hardware::State::INIT );
				}
			
				g_logger->log( Logger::LogLevel::VERBOSE, this, "Device added." );
			}
		} );
#endif // _WITH_LIBUDEV

		Hardware::start();
	};

	void ZWave::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		g_webServer->removeResourceCallback( "zwave-" + std::to_string( this->m_id ) );

		std::unique_lock<std::timed_mutex> lock( ZWave::g_managerMutex );
		Manager::Get()->RemoveWatcher( micasa_openzwave_notification_handler, this );
		if ( ! this->m_port.empty() ) {
			Manager::Get()->RemoveDriver( this->m_port );
			this->m_port.clear();
		}
		g_managerWatchers--;
		if ( 0 == g_managerWatchers ) {
			Manager::Destroy();
			Options::Destroy();
		}
		lock.unlock();

#ifdef _WITH_LIBUDEV
		g_controller->removeSerialPortCallback( "openzwave_" + this->m_id );
#endif // _WITH_LIBUDEV

		Hardware::stop();
	};

	std::string ZWave::getLabel() const {
		return this->m_settings->get( "label", "Z-Wave" );
	};

	json ZWave::getJson( bool full_ ) const {
		if ( full_ ) {
			json setting = {
				{ "name", "port" },
				{ "label", "Port" },
				{ "type", "string" },
				{ "value", this->m_settings->get( "port", "" ) }
			};

#ifdef _WITH_LIBUDEV
			json options = json::array();
			auto ports = getSerialPorts();
			for ( auto portsIt = ports.begin(); portsIt != ports.end(); portsIt++ ) {
				json option = json::object();
				option["value"] = portsIt->first;
				option["label"] = portsIt->second;
				options += option;
			}

			setting["type"] = "list";
			setting["options"] = options;
#endif // _WITH_LIBUDEV

			json result = Hardware::getJson( full_ );
			result["settings"] = { setting };
			return result;
		} else {
			return Hardware::getJson( full_ );
		}
	};

	void ZWave::_handleNotification( const Notification* notification_ ) {
		if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {

#ifdef _DEBUG
			g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );
#endif // _DEBUG

			unsigned int homeId = notification_->GetHomeId();
			unsigned int nodeId = notification_->GetNodeId();

			// Notifications for specific nodes are handled by the node hardware.
			std::stringstream reference;
			reference << homeId << "/" << (unsigned int)nodeId;
			auto node = std::static_pointer_cast<ZWaveNode>( g_controller->getHardware( reference.str() ) );
			if (
				node != nullptr
				&& node->isRunning()
			) {
				node->_handleNotification( notification_ );
			}

			// Handle certain controller- or driver notifications ourselves.
			switch( notification_->GetType() ) {

				case Notification::Type_DriverReady: {
					if (
						this->m_homeId == 0
						|| this->m_homeId == homeId
					) {
						this->m_homeId = homeId;
						this->m_settings->put( "port", this->m_port );
						this->m_settings->put<unsigned int>( "home_id", homeId );
						g_logger->log( Logger::LogLevel::NORMAL, this, "Driver ready." );
					} else {
						Manager::Get()->RemoveDriver( this->m_port );
						this->m_port.clear();
						g_logger->log( Logger::LogLevel::ERROR, this, "Driver has wrong home id." );
					}
					break;
				}

				case Notification::Type_DriverFailed: {
					this->setState( Hardware::State::FAILED );
					if ( this->m_port == this->m_settings->get( "port" ) ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Driver failed." );
					}
					Manager::Get()->RemoveDriver( this->m_port );
					this->m_port.clear();
					break;
				}

				case Notification::Type_AwakeNodesQueried:
				case Notification::Type_AllNodesQueried:
				case Notification::Type_AllNodesQueriedSomeDead: {
					this->setState( Hardware::State::READY );
					break;
				}

				case Notification::Type_ControllerCommand: {
					switch( notification_->GetEvent() ) {
						case Driver::ControllerState_Cancel: {
							break;
						}
						case Driver::ControllerState_Error: {
							break;
						}
						case Driver::ControllerState_Completed: {
							break;
						}
						case Driver::ControllerState_Failed: {
							break;
						}
					}
					break;
				}

				case Notification::Type_NodeProtocolInfo: {
					if (
						homeId == this->m_homeId
						&& nodeId > 1 // skip the controller (most likely node 1)
					) {
						std::string label = Manager::Get()->GetNodeType( homeId, nodeId );
						g_controller->declareHardware( Hardware::Type::ZWAVE_NODE, reference.str(), this->shared_from_this(), {
							{ "label", label },
							{ "home_id", std::to_string( homeId ) },
							{ "node_id", std::to_string( nodeId ) }
						}, true /* auto start */ );
					}
				}

				case Notification::Type_NodeNaming: {
					if (
						homeId == this->m_homeId
						&& nodeId == 1 // most likely the controller node
					) {
						std::string manufacturer = Manager::Get()->GetNodeManufacturerName( homeId, nodeId );
						std::string product = Manager::Get()->GetNodeProductName( homeId, nodeId );
						std::string nodeName = Manager::Get()->GetNodeName( homeId, nodeId );
						if ( ! manufacturer.empty() ) {
							this->m_settings->put( "label", manufacturer + " " + product );
						}
						if (
							! nodeName.empty()
							&& this->m_settings->get( "name", "" ).empty()
						) {
							this->m_settings->put( "name", nodeName );
						}
					}
					break;
				}

				case Notification::Type_NodeRemoved: {
					// The NodeRemove notification is a bit tricky because it is also called upon application close.
					// We're using it to remove hardware anyhow because the watcher is removed prior to the driver
					// being removed.
					if ( node != nullptr ) {
						g_controller->removeHardware( node );
					}
					break;
				};

				case Notification::Type_ValueRemoved: {
					// According to the documentation, this notification only occurs when a node is removed from the
					// network, as apposed to the Type_NodeRemoved notification which also occurs when the application
					// is shutting down.
					break;
				}

				default: {
					break;
				}
			}

			ZWave::g_managerMutex.unlock();

		} else {
			g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave notification \"%s\" missed.", notification_->GetAsString().c_str() );
		}
	};

	void ZWave::_installResourceHandlers() const {
		g_webServer->removeResourceCallback( "zwave-" + std::to_string( this->m_id ) );

		// Add resource handlers for network heal.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"zwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/heal", 101,
			WebServer::Method::PUT,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex, std::adopt_lock );
					if ( this->getState() == Hardware::State::READY ) {
						Manager::Get()->HealNetwork( this->m_homeId, true );
						output_["result"] = "OK";
						g_logger->log( Logger::LogLevel::NORMAL, this, "Network heal initiated." );
					} else {
						output_["result"] = "ERROR";
						output_["message"] = "Controller not ready.";
						code_ = 423; // Locked (WebDAV; RFC 4918)
					}
				} else {
					output_["result"] = "ERROR";
					output_["message"] = "Controller busy.";
					code_ = 423; // Locked (WebDAV; RFC 4918)
				}
			} )
		} ) ) );

		// Add resource handler for inclusion mode.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"zwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/include", 101,
			WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				// TODO also accept secure inclusion mode
				if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex, std::adopt_lock );
					if ( method_ == WebServer::Method::PUT ) {
						if ( this->getState() == Hardware::State::READY ) {
							if ( Manager::Get()->AddNode( this->m_homeId, false ) ) {
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
							Manager::Get()->CancelControllerCommand( this->m_homeId );
							output_["result"] = "OK";
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Controller not ready.";
							code_ = 409; // Conflict
						}
					}
				} else {
					output_["result"] = "ERROR";
					output_["message"] = "Controller busy.";
					code_ = 423; // Locked (WebDAV; RFC 4918)
				}
			} )
		} ) ) );

		// Add resource handler for exclusion mode.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"zwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/exclude", 101,
			WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex, std::adopt_lock );
					if ( method_ == WebServer::Method::PUT ) {
						if ( this->getState() == Hardware::State::READY ) {
							if ( Manager::Get()->RemoveNode( this->m_homeId ) ) {
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
							Manager::Get()->CancelControllerCommand( this->m_homeId );
							output_["result"] = "OK";
						} else {
							output_["result"] = "ERROR";
							output_["message"] = "Controller not ready.";
							code_ = 409; // Conflict
						}
					}
				} else {
					output_["result"] = "ERROR";
					output_["message"] = "Controller busy.";
					code_ = 423; // Locked (WebDAV; RFC 4918)
				}
			} )
		} ) ) );
	};

}; // namespace micasa
