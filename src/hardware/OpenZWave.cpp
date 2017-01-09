// https://github.com/OpenZWave/open-zwave/wiki/Config-Options
// http://www.openzwave.com/dev/classOpenZWave_1_1Notification.html#a5fa14ba721a25a4c84e0fbbedd767d54a4432a88465416a63cf4eda11ecf28c24

// pipes to store and retrieve the log and config?
// http://stackoverflow.com/questions/2784500/how-to-send-a-simple-string-between-two-programs-using-pipes

#include "OpenZWave.h"
#include "OpenZWaveNode.h"

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
	auto target = reinterpret_cast<micasa::OpenZWave*>( handler_ );
	target->_handleNotification( notification_ );
};

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	OpenZWave::OpenZWave( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		// The settings for OpenZWave need to be entered before the hardware is started. Therefore the
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

	void OpenZWave::start() {

		// At this point there should be a valid z-wave manager we can use for this piece of hardware.
		if ( ! this->m_settings->contains( { "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "OpenZWave Version %s.", ::OpenZWave::Manager::getVersionAsString().c_str() );
	
		// There's only one openzwave manager, so we need to create it statically and share it among
		// z-wave hardware if there are more than one.
		{
			static std::mutex mutex;
			std::lock_guard<std::mutex> lock( mutex );
			
			static unsigned int managerStatus = 0;
			if ( managerStatus == 0 ) {
		
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
					managerStatus = 1;
				} else {
					managerStatus = 2;
				}
	
			} else if ( managerStatus == 2 ) {
				this->setState( Hardware::State::FAILED );
				return;
			}
		}

		::OpenZWave::Manager::Get()->AddWatcher( micasa_openzwave_notification_handler, this );
		::OpenZWave::Manager::Get()->AddDriver( this->m_settings->get( "port" ) );

		this->m_port = this->m_settings->get( "port" );
		this->m_homeId = this->m_settings->get<unsigned int>( "home_id", 0 );

#ifdef _WITH_LIBUDEV
		// If libudev is available we can detect if a device is added or removed from the system.
		// TODO make this more rebust.
		g_controller->addSerialPortCallback( "openzwave_" + this->m_id, [this]( const std::string& serialPort_, const std::string& action_ ) {

			// If the serial port we're currently using was removed, set the device state to disabled.
			if (
				serialPort_ == this->m_port
				&& action_ == "remove"
			) {
				::OpenZWave::Manager::Get()->RemoveDriver( serialPort_ );
				this->setState( Hardware::State::DISCONNECTED );
				auto children = g_controller->getChildrenOfHardware( *this );
				for ( auto childrenIt = children.begin(); childrenIt != children.end(); childrenIt++ ) {
					(*childrenIt)->setState( Hardware::State::DISCONNECTED );
				}
				g_logger->log( Logger::LogLevel::WARNING, this, "Disconnected." );
			}
			
			// If a serial port was added (not necessarely ours) and we're currently disabled, see if it
			// is the device we should use.
			if (
				action_ == "add"
				&& (
					this->getState() == Hardware::State::DISCONNECTED
					|| this->getState() == Hardware::State::FAILED
				)
				&& ::OpenZWave::Manager::Get()->AddDriver( serialPort_ )
			) {
				this->setState( Hardware::State::INIT );
				auto children = g_controller->getChildrenOfHardware( *this );
				for ( auto childrenIt = children.begin(); childrenIt != children.end(); childrenIt++ ) {
					(*childrenIt)->setState( Hardware::State::INIT );
				}
				this->m_port = serialPort_;
				g_logger->log( Logger::LogLevel::VERBOSE, this, "Device added." );
			}
		} );
#endif // _WITH_LIBUDEV

		Hardware::start();
	};

	void OpenZWave::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

#ifdef _WITH_LIBUDEV
		g_controller->removeSerialPortCallback( "openzwave_" + this->m_id );
#endif // _WITH_LIBUDEV

		::OpenZWave::Manager::Get()->RemoveWatcher( micasa_openzwave_notification_handler, this );
		if ( this->m_port != "" ) {
			::OpenZWave::Manager::Get()->RemoveDriver( this->m_port );
		}
		g_webServer->removeResourceCallback( "openzwave-" + std::to_string( this->m_id ) );

		Hardware::stop();
	};

	std::string OpenZWave::getLabel() const {
		return this->m_settings->get( "label", "OpenZWave" );
	};

	json OpenZWave::getJson( bool full_ ) const {
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

	std::chrono::milliseconds OpenZWave::_work( const unsigned long int& iteration_ ) {
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	};

	void OpenZWave::_handleNotification( const ::OpenZWave::Notification* notification_ ) {
		std::unique_lock<std::timed_mutex> lock( this->m_notificationMutex );
		
#ifdef _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );
#endif // _DEBUG

		unsigned int homeId = notification_->GetHomeId();
		unsigned char nodeId = notification_->GetNodeId();
		
		// Notifications for specific nodes are handled by the node hardware.
		std::stringstream reference;
		reference << homeId << "/" << (unsigned int)nodeId;
		auto node = std::static_pointer_cast<OpenZWaveNode>( g_controller->getHardware( reference.str() ) );
		if (
			node != nullptr
			&& node->isRunning()
		) {
			node->_handleNotification( notification_ );
		} else if (
			nodeId > 0
			&& this->m_controllerNodeId != nodeId
		) {
			g_logger->logr( Logger::LogLevel::WARNING, this, "Notification for unknown node %d discarded.", nodeId );
		}

		// Handle controller notification ourselves.
		switch( notification_->GetType() ) {

			case ::OpenZWave::Notification::Type_DriverReady: {
				if (
					this->m_homeId == 0
					|| this->m_homeId == homeId
				) {
					this->m_homeId = homeId;
					this->m_controllerNodeId = nodeId;
					this->m_settings->put( "port", this->m_port );
					this->m_settings->put<unsigned int>( "home_id", homeId );
					g_logger->log( Logger::LogLevel::NORMAL, this, "Driver initialized." );
				} else {
					g_logger->log( Logger::LogLevel::ERROR, this, "Driver has wrong home id." );
					// The remove driver immediately triggers a notification in the same thread causing a
					// deadlock of the lock is not released.
					lock.unlock();
					::OpenZWave::Manager::Get()->RemoveDriver( this->m_port );
					this->m_port = "";
				}
				break;
			}

			case ::OpenZWave::Notification::Type_DriverFailed: {
				this->setState( Hardware::State::FAILED );
				if ( this->m_port == this->m_settings->get( "port" ) ) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Driver failed." );
				}
				// The remove driver immediately triggers a notification in the same thread causing a
				// deadlock of the lock is not released.
				lock.unlock();
				::OpenZWave::Manager::Get()->RemoveDriver( this->m_port );
				this->m_port = "";
				break;
			}

			case ::OpenZWave::Notification::Type_AwakeNodesQueried:
			case ::OpenZWave::Notification::Type_AllNodesQueried:
			case ::OpenZWave::Notification::Type_AllNodesQueriedSomeDead: {
				this->setState( Hardware::State::READY );
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
						{ "home_id", std::to_string( homeId ) },
						{ "node_id", std::to_string( nodeId ) }
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
					std::string nodeName = ::OpenZWave::Manager::Get()->GetNodeName( homeId, nodeId );
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

			case ::OpenZWave::Notification::Type_ValueRemoved: {
				// According to the documentation, this notification only occurs when a node is removed from the
				// network, as apposed to the Type_NodeRemoved notification which also occurs when the application
				// is shutting down.
				if ( node != nullptr ) {
					g_controller->removeHardware( node );
				}
				break;
			}

			default: {
				break;
			}
		}
	};

	void OpenZWave::_installResourceHandlers() const {

		// Remove previously installed callbacks.
		g_webServer->removeResourceCallback( "openzwave-" + std::to_string( this->m_id ) );
	
		// Add resource handlers for network heal.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"openzwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/heal", 101,
			WebServer::Method::PUT,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( this->m_notificationMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( this->m_notificationMutex, std::adopt_lock );
					if ( this->getState() == Hardware::State::READY ) {
						::OpenZWave::Manager::Get()->HealNetwork( this->m_homeId, true );
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
			"openzwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/include", 101,
			WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				// TODO also accept secure inclusion mode
				// TODO cancel inclusion mode after xx minutes? openzwave doesn't cancel
				if ( this->m_notificationMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( this->m_notificationMutex, std::adopt_lock );
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
							::OpenZWave::Manager::Get()->CancelControllerCommand( this->m_homeId );
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
			"openzwave-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/exclude", 101,
			WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				// TODO cancel exclusion mode after xx minutes? openzwave doesn't cancel
				if ( this->m_notificationMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( this->m_notificationMutex, std::adopt_lock );
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
							::OpenZWave::Manager::Get()->CancelControllerCommand( this->m_homeId );
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
