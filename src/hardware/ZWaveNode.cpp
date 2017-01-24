#include <chrono>

#include "ZWaveNode.h"
#include "ZWave/CommandClasses.h"

#include "../Logger.h"
#include "../Controller.h"
#include "../WebServer.h"
#include "../User.h"
#include "../Utils.h"

#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Counter.h"
#include "../device/Switch.h"

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

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	void ZWaveNode::start() {
#ifdef _DEBUG
		assert( this->m_settings->contains( { "home_id", "node_id" } ) && "ZWaveNode should be declared with home_id and node_id." );
#endif // _DEBUG

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		this->m_homeId = this->m_settings->get<unsigned int>( "home_id" );
		this->m_nodeId = this->m_settings->get<unsigned int>( "node_id" );

		this->_installResourceHandlers();

		// If the parent z-wave hardware is already running we can immediately set the state of this node
		// to ready.
		if ( this->getParent()->getState() == Hardware::State::READY ) {
			this->setState( Hardware::State::READY );
		}

		Hardware::start();
	};

	void ZWaveNode::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		g_webServer->removeResourceCallback( "zwavenode-" + std::to_string( this->m_id ) );
		Hardware::stop();
	};

	std::string ZWaveNode::getLabel() const {
		return this->m_settings->get( "label", "Z-Wave Node" );
	};

	json ZWaveNode::getJson( bool full_ ) const {
		if ( full_ ) {
			std::lock_guard<std::mutex> lock( this->m_configurationMutex );
			json result = Hardware::getJson( full_ );
			result["settings"] = json::array();
			for( auto const& setting: this->m_configuration ) {
				result["settings"]+= setting;
			}
			return result;
		} else {
			return Hardware::getJson( full_ );
		}
	}

	bool ZWaveNode::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		apply_ = false;

		if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
			std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex, std::adopt_lock );

			if ( this->getState() != Hardware::State::READY ) {
				if ( this->getState() == Hardware::State::FAILED ) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Node is dead." );
					return false;
				} else if ( this->getState() == Hardware::State::SLEEPING ) {
					g_logger->log( Logger::LogLevel::WARNING, this, "Node is sleeping." );
					// fallthrough, event can be sent regardless of sleeping state.
				} else {
					g_logger->log( Logger::LogLevel::ERROR, this, "Node is busy." );
					return false;
				}
			}

			// Reconstruct the value id which is needed to send a command to a node.
			ValueID valueId( this->m_homeId, std::stoull( device_->getReference() ) );
			switch( valueId.GetCommandClassId() ) {
				case COMMAND_CLASS_SWITCH_BINARY: {
					if (
						valueId.GetType() == ValueID::ValueType_Bool
						&& device_->getType() == Device::Type::SWITCH
					) {
						// TODO differentiate between blinds, switches etc (like open close on of etc).
						if ( this->_queuePendingUpdate( device_->getReference(), source_, OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC, OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC ) ) {
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
							bool value = ( device->getValueOption() == Switch::Option::ON ) ? true : false;
							return Manager::Get()->SetValue( valueId, value );
						} else {
							g_logger->log( Logger::LogLevel::ERROR, this, "Node busy." );
							return false;
						}
					}
				}
				case COMMAND_CLASS_SWITCH_MULTILEVEL: {
					if (
						valueId.GetType() == ValueID::ValueType_Byte
						&& device_->getType() == Device::Type::LEVEL
					) {
						if ( this->_queuePendingUpdate( device_->getReference(), source_, OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC, OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC ) ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( device_ );
							return Manager::Get()->SetValue( valueId, uint8( device->getValue() ) );
						} else {
							g_logger->log( Logger::LogLevel::ERROR, this, "Node busy." );
							return false;
						}
					}
				}

			}
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid command." );
			return false;

		} else {
			g_logger->log( Logger::LogLevel::ERROR, this, "Controller busy." );
			return false;
		}

		return true;
	};

	void ZWaveNode::_handleNotification( const Notification* notification_ ) {
		// No need to lock here, the parent ZWave instance already holds a lock while proxying the
		// notification to us.

#ifdef _DEBUG
		assert( this->m_homeId == notification_->GetHomeId() && "ZWaveNode home_id should be correct." );
		assert( this->m_nodeId == notification_->GetNodeId() && "ZWaveNode node_id should be correct." );
#endif // _DEBUG

		switch( notification_->GetType() ) {

			case Notification::Type_EssentialNodeQueriesComplete: {
				if ( this->getState() == Hardware::State::INIT ) {
					g_logger->log( Logger::LogLevel::NORMAL, this, "Node ready." );
					this->setState( Hardware::State::READY );
				}
				this->_updateNames();
				break;
			}

			case Notification::Type_NodeQueriesComplete: {
				this->_updateNames();
				break;
			}

			case Notification::Type_ValueAdded: {
				ValueID valueId = notification_->GetValueID();
				this->_processValue( valueId, Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE );
				break;
			}

			case Notification::Type_ValueChanged: {
				ValueID valueId = notification_->GetValueID();
				this->_processValue( valueId, Device::UpdateSource::HARDWARE );
				break;
			}

			case Notification::Type_Notification: {
				switch( notification_->GetNotification() ) {
					case Notification::Code_Alive: {
						if ( this->getState() == Hardware::State::FAILED ) {
							g_logger->log( Logger::LogLevel::NORMAL, this, "Node is alive again." );
						}
						this->_updateNames();
						this->setState( Hardware::State::READY );
						break;
					}
					case Notification::Code_Dead: {
						g_logger->log( Logger::LogLevel::ERROR, this, "Node is dead." );
						this->setState( Hardware::State::FAILED );
						break;
					}
					case Notification::Code_Awake: {
						if ( this->getState() == Hardware::State::SLEEPING ) {
							g_logger->log( Logger::LogLevel::NORMAL, this, "Node is awake again." );
						}
						this->setState( Hardware::State::READY );
						this->_updateNames();
						break;
					}
					case Notification::Code_Sleep: {
						g_logger->log( Logger::LogLevel::NORMAL, this, "Node is asleep." );
						this->setState( Hardware::State::SLEEPING );
						break;
					}
					case Notification::Code_Timeout: {
						g_logger->log( Logger::LogLevel::WARNING, this, "Node timeout." );
						break;
					}
				}
				break;
			}

			case Notification::Type_NodeProtocolInfo: {
				this->_updateNames();
			}

			case Notification::Type_NodeNaming: {
				this->_updateNames();
				break;
			}

			default: {
				break;
			}
		}
	};

	void ZWaveNode::_processValue( const ValueID& valueId_, Device::UpdateSource source_ ) {
		std::string label = Manager::Get()->GetValueLabel( valueId_ );
		std::string reference = std::to_string( valueId_.GetId() );
		std::string units = Manager::Get()->GetValueUnits( valueId_ );
		
		//ValueID::ValueGenre genre = valueId_.GetGenre();

		// Some values are not going to be processed ever and can be filtered out beforehand.
		if (
			"Exporting" == label
			|| "Interval" == label
			|| "Previous Reading" == label
			|| "Library Version" == label
			|| "Protocol Version" == label
			|| "Application Version" == label
			|| "Previous Reading" == label
			|| "Power Factor" == label
		) {
			return;
		}

		// Process all other values by command class.
		unsigned int commandClass = valueId_.GetCommandClassId();
		switch( commandClass ) {

			case COMMAND_CLASS_POWERLEVEL: {
				// This is actually not ONE command class but several; Powerlevel, Timeout, Set Powerlevel, Test Node,
				// Test Powerlevel, Frame Count, Test, Report, Test Status, Acked Frames and possibly others. They are
				// used to keep the mesh network in optimal state and do not provide any meaningfull information for
				// the end user. They are thus ignored.
				break;
			}

			case COMMAND_CLASS_PROTECTION: // not yet implemented
			case COMMAND_CLASS_NO_OPERATION:
			case COMMAND_CLASS_SWITCH_ALL:
			case COMMAND_CLASS_ZWAVE_PLUS_INFO: {
				// Not implemented yet.
				break;
			}

			case COMMAND_CLASS_BASIC: {
				// https://github.com/OpenZWave/open-zwave/wiki/Basic-Command-Class
				// Conclusion; try to use devices that send other cc values aswell as the basic set. For instance,
				// the Aeotec ZW089 Recessed Door Sensor Gen5 defaults to only sending basic messages BUT can be
				// configured to also send binary reports.
				break;
			}

			case COMMAND_CLASS_SWITCH_BINARY:
			case COMMAND_CLASS_SENSOR_BINARY: {
				// Detect subtype.
				// TODO improve detection
				auto subtype = Switch::SubType::GENERIC;
				auto hardwareLabel = this->getLabel();
				std::transform( hardwareLabel.begin(), hardwareLabel.end(), hardwareLabel.begin(), ::tolower );
				if ( hardwareLabel.find( "pir" ) != string::npos ) {
					subtype = Switch::SubType::MOTION_DETECTOR;
				} else if ( hardwareLabel.find( "motion" ) != string::npos ) {
					subtype = Switch::SubType::MOTION_DETECTOR;
				} else if ( hardwareLabel.find( "home security" ) != string::npos ) {
					subtype = Switch::SubType::MOTION_DETECTOR;
				} else if ( hardwareLabel.find( "door" ) != string::npos ) {
					subtype = Switch::SubType::DOOR_CONTACT;
				}
			
				// TODO if a switch comes too soon after a manual switch (not from hardware) ignore- or revert it.
				// TODO this prevents having to code javascript to ignore switches from happing right after a PIR
				// instruction.
				Device::UpdateSource allowedUpdateSources = Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE;
				if ( commandClass == COMMAND_CLASS_SWITCH_BINARY ) {
					allowedUpdateSources |= Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API;

					// If there's an update mutex available we need to make sure that it is properly notified of the
					// execution of the update.
					source_ |= this->_releasePendingUpdate( reference );
				}
				bool boolValue = false;
				unsigned char byteValue = 0;
				if (
					valueId_.GetType() == ValueID::ValueType_Bool
					&& false != Manager::Get()->GetValueAsBool( valueId_, &boolValue )
				) {
					// TODO differentiate between blinds, switches etc (like open close on of etc).
					auto device = this->_declareDevice<Switch>( reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( allowedUpdateSources ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE, Switch::resolveSubType( subtype ) },
						{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true }
					} );
					device->updateValue( source_, boolValue ? Switch::Option::ON : Switch::Option::OFF );
					if ( "Unknown" != label ) {
						device->setLabel( label );
					}
				}
				if (
					valueId_.GetType() == ValueID::ValueType_Byte
					&& false != Manager::Get()->GetValueAsByte( valueId_, &byteValue )
				) {
					// TODO differentiate between blinds, switches etc (like open close on of etc).
					auto device = this->_declareDevice<Switch>( reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( allowedUpdateSources ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE, Switch::resolveSubType( subtype ) },
						{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true }
					} );
					device->updateValue( source_, byteValue ? Switch::Option::ON : Switch::Option::OFF );
					if ( "Unknown" != label ) {
						device->setLabel( label );
					}
				}
				break;
			}

			case COMMAND_CLASS_SWITCH_MULTILEVEL: {
				// For now only level multilevel devices are supported.
				// NOTE: for instance, the fibaro dimmer has several multilevel devices, such as start level-,
				// step size and dimming duration. These should be handled through the configuration.
				if ( "Level" == label ) {
					// Detect subtype.
					// TODO improve detection
					auto subtype = Level::SubType::GENERIC;
					auto hardwareLabel = this->getLabel();
					std::transform( hardwareLabel.begin(), hardwareLabel.end(), hardwareLabel.begin(), ::tolower );
					if ( hardwareLabel.find( "dimmer" ) != string::npos ) {
						subtype = Level::SubType::DIMMER;
					}
				
					Device::UpdateSource allowedUpdateSources = Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API;

					// If there's an update mutex available we need to make sure that it is properly notified of the
					// execution of the update.
					source_ |= this->_releasePendingUpdate( reference );

					unsigned char byteValue = 0;

					if (
						valueId_.GetType() == ValueID::ValueType_Byte
						&& false != Manager::Get()->GetValueAsByte( valueId_, &byteValue )
					) {
						auto device = this->_declareDevice<Level>( reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( allowedUpdateSources ) },
							{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::PERCENT ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( subtype ) },
							{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, true }
						} );
						device->updateValue( source_, byteValue );
						if ( "Unknown" != label ) {
							device->setLabel( label );
						}
					}
				}
			}

			case COMMAND_CLASS_METER:
			case COMMAND_CLASS_SENSOR_MULTILEVEL: {
				float floatValue = 0;
				if (
					valueId_.GetType() == ValueID::ValueType_Decimal
					&& false != Manager::Get()->GetValueAsFloat( valueId_, &floatValue )
				) {
					// TODO check units to see if any multiplication needs to take place (from watt kwh)?

					if (
						"Energy" == label
						|| "Gas" == label
						|| "Water" == label
					) {
						auto subtype = Counter::SubType::GENERIC;
						auto unit = Counter::Unit::GENERIC;
						if ( "Energy" == label ) {
							subtype = Counter::SubType::ENERGY;
							unit = Counter::Unit::KILOWATTHOUR;
						} else if ( "Gas" == label ) {
							subtype = Counter::SubType::GAS;
							unit = Counter::Unit::M3;
						} else if ( "Water" == label ) {
							subtype = Counter::SubType::WATER;
							unit = Counter::Unit::M3;
						}
						auto device = this->_declareDevice<Counter>( reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE, Counter::resolveSubType( subtype ) },
							{ DEVICE_SETTING_DEFAULT_UNITS, Counter::resolveUnit( unit ) }
						} );
						device->updateValue( source_, floatValue );
					} else {
						auto subtype = Level::SubType::GENERIC;
						auto unit = Level::Unit::GENERIC;
						if ( "Power" == label ) {
							subtype = Level::SubType::POWER;
							unit = Level::Unit::WATT;
						} else if ( "Voltage" == label ) {
							subtype = Level::SubType::ELECTRICITY;
							unit = Level::Unit::VOLT;
						} else if ( "Current" == label ) {
							subtype = Level::SubType::CURRENT;
							unit = Level::Unit::AMPERES;
						} else if ( "Temperature" == label ) {
							subtype = Level::SubType::TEMPERATURE;
							unit = Level::Unit::CELSIUS;
						} else if ( "Luminance" == label ) {
							subtype = Level::SubType::LUMINANCE;
							unit = Level::Unit::LUX;
						}
						auto device = this->_declareDevice<Level>( reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( subtype ) },
							{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( unit ) }
						} );
						device->updateValue( source_, floatValue );
					}
				}
				break;
			}

			case COMMAND_CLASS_BATTERY: {
				unsigned char byteValue = 0;
				if (
					valueId_.GetType() == ValueID::ValueType_Byte
					&& false != Manager::Get()->GetValueAsByte( valueId_, &byteValue )
				) {
					auto device = this->_declareDevice<Level>( reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::BATTERY_LEVEL ) },
						{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::PERCENT ) }
					} );
					device->updateValue( source_, (unsigned int)byteValue );
				}
				break;
			}

			case COMMAND_CLASS_WAKE_UP:
			case COMMAND_CLASS_CONFIGURATION: {
				std::lock_guard<std::mutex> lock( this->m_configurationMutex );
			
				if (
					commandClass == COMMAND_CLASS_WAKE_UP
					&& label != "Wake-up Interval"
				) {
					break;
				}
				
				// The configuration is stored with the reference as both the key and as the name property. This
				// comes in handy in both cases where the configuration is needed, that is when adding it to the
				// json and when processing settings from the callback.

				// Some configuration parameters refer to other paramters by index.
				unsigned int index = valueId_.GetIndex();
				if ( index > 0 ) {
					label = std::to_string( index ) + ". " + label;
				}

				json setting = {
					{ "name", reference },
					{ "label", label },
					{ "description", Manager::Get()->GetValueHelp( valueId_ ) },
					{ "class", "advanced" }
				};

				ValueID::ValueType type = valueId_.GetType();
				
				if ( type == ValueID::ValueType_Decimal ) {
					setting["type"] = "double";
					setting["min"] = Manager::Get()->GetValueMin( valueId_ );
					if ( Manager::Get()->GetValueMax( valueId_ ) > Manager::Get()->GetValueMin( valueId_ ) ) {
						setting["max"] = Manager::Get()->GetValueMax( valueId_ );
					} else {
						setting["max"] = std::numeric_limits<double>::max();
					}
					float floatValue = 0;
					Manager::Get()->GetValueAsFloat( valueId_, &floatValue );
					setting["value"] = floatValue;
				} else if ( type == ValueID::ValueType_Bool ) {
					setting["type"] = "boolean";
					bool boolValue = 0;
					Manager::Get()->GetValueAsBool( valueId_, &boolValue );
					setting["value"] = boolValue;
				} else if ( type == ValueID::ValueType_Byte ) {
					setting["type"] = "byte";
					setting["min"] = Manager::Get()->GetValueMin( valueId_ );
					if ( Manager::Get()->GetValueMax( valueId_ ) > Manager::Get()->GetValueMin( valueId_ ) ) {
						setting["max"] = Manager::Get()->GetValueMax( valueId_ );
					} else {
						setting["max"] = std::numeric_limits<unsigned char>::max();
					}
					unsigned char byteValue = 0;
					Manager::Get()->GetValueAsByte( valueId_, &byteValue );
					setting["value"] = byteValue;
				} else if ( type == ValueID::ValueType_Short ) {
					setting["type"] = "short";
					setting["min"] = Manager::Get()->GetValueMin( valueId_ );
					if ( Manager::Get()->GetValueMax( valueId_ ) > Manager::Get()->GetValueMin( valueId_ ) ) {
						setting["max"] = Manager::Get()->GetValueMax( valueId_ );
					} else {
						setting["max"] = std::numeric_limits<short>::max();
					}
					short shortValue = 0;
					Manager::Get()->GetValueAsShort( valueId_, &shortValue );
					setting["value"] = shortValue;
				} else if ( type == ValueID::ValueType_Int ) {
					setting["type"] = "int";
					setting["min"] = Manager::Get()->GetValueMin( valueId_ );
					if ( Manager::Get()->GetValueMax( valueId_ ) > Manager::Get()->GetValueMin( valueId_ ) ) {
						setting["max"] = Manager::Get()->GetValueMax( valueId_ );
					} else {
						setting["max"] = std::numeric_limits<int>::max();
					}
					int intValue = 0;
					Manager::Get()->GetValueAsInt( valueId_, &intValue );
					setting["value"] = intValue;
				} else if ( type == ValueID::ValueType_Button ) {
					// Perform action, such as reset to factory defaults.
					// TODO these should actually be action resources directly, but how to name the resource?
					//setting["type"] = "button";
					break; // skip this setting for now > unsupported
				} else if ( type == ValueID::ValueType_List ) {
					setting["type"] = "list";
					std::string listValue;
					Manager::Get()->GetValueListSelection( valueId_, &listValue );
					setting["value"] = listValue;

					setting["options"] = json::array();
					std::vector<std::string> items;
					Manager::Get()->GetValueListItems( valueId_, &items );
					for ( auto itemsIt = items.begin(); itemsIt != items.end(); itemsIt++ ) {
						json option = json::object();
						option["value"] = (*itemsIt);
						option["label"] = (*itemsIt);
						setting["options"] += option;
					}
				} else if ( type == ValueID::ValueType_String ) {
					setting["type"] = "string";
					std::string stringValue;
					Manager::Get()->GetValueAsString( valueId_, &stringValue );
					setting["value"] = stringValue;
				}
				
				// If the variable is readonly it's type is overridden to display, meaning that the client should
				// show the value for informational purpose only.
				if ( Manager::Get()->IsValueReadOnly( valueId_ ) ) {
					setting["type"] = "display";
					setting["readonly"] = true;
				} else {
					setting["readonly"] = false;
				}

				this->m_configuration[reference] = setting;
				break;
			}

			case COMMAND_CLASS_ALARM:
			case COMMAND_CLASS_SENSOR_ALARM: {
				// https://github.com/OpenZWave/open-zwave/wiki/Alarm-Command-Class
				break;
			}

			default: {
				break;
			}
		}
	};

	void ZWaveNode::_updateNames() {
		std::string type = Manager::Get()->GetNodeType( this->m_homeId, this->m_nodeId );
		std::string manufacturer = Manager::Get()->GetNodeManufacturerName( this->m_homeId, this->m_nodeId );
		std::string product = Manager::Get()->GetNodeProductName( this->m_homeId, this->m_nodeId );
		if ( ! manufacturer.empty() ) {
			this->m_settings->put( "label", manufacturer + " " + product );
		} else if ( ! type.empty() ) {
			this->m_settings->put( "label", type );
		}

		std::string nodeName = Manager::Get()->GetNodeName( this->m_homeId, this->m_nodeId );
		if ( ! nodeName.empty() ) {
			if ( this->m_settings->get( "name", "" ).empty() ) {
				// If no name has been entered yet for this hardware, use the one from openzwave.
				this->m_settings->put( "name", nodeName );
			} else if ( nodeName != this->m_settings->get( "name" ) ) {
				// Make sure the configured hardware name is the same as the node name used by
				// openzwave and stored in the config xml file.
				Manager::Get()->SetNodeName( this->m_homeId, this->m_nodeId, this->m_settings->get( "name" ) );
			}
		}
		
		this->m_settings->commit();
	};

	void ZWaveNode::_installResourceHandlers() {
		g_webServer->removeResourceCallback( "zwavenode-" + std::to_string( this->m_id ) );

		// Add resource handlers for network heal.
		// http://www.openzwave.com/knowledge-base/deadnode
		g_webServer->addResourceCallback( {
			"zwavenode-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "/heal$",
			101,
			WebServer::Method::PUT,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				std::shared_ptr<ZWave> parent = std::static_pointer_cast<ZWave>( this->m_parent );
				if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex, std::adopt_lock );
					if ( parent->getState() == Hardware::State::READY ) {
						Manager::Get()->HealNetworkNode( this->m_homeId, this->m_nodeId, true );
						g_logger->log( Logger::LogLevel::NORMAL, this, "Node heal initiated." );
					} else {
						g_logger->log( Logger::LogLevel::ERROR, this, "Controller not ready." );
						throw WebServer::ResourceException( 423, "Hardware.Not.Ready", "The hardware is not ready." );
					}
					output_["code"] = 200;
				} else {
					g_logger->log( Logger::LogLevel::ERROR, this, "Controller busy." );
					throw WebServer::ResourceException( 423, "Hardware.Busy", "The hardware is busy." );
				}
			} )
		} );
		
		// Add resource handler for settings update.
		g_webServer->addResourceCallback( {
			"zwavenode-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "$",
			99,
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				// Obtain a lock on the static/shared OpenZWave library manager. If it takes too long to
				// obtain the lock, the command will fail.
				if ( ZWave::g_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( ZWave::g_managerMutex, std::adopt_lock );

					// Set the node name so it's persistant. This name is stored in the xml config file.
					auto find = input_.find( "name" );
					if ( find != input_.end() ) {
						if ( (*find).is_string() ) {
							Manager::Get()->SetNodeName( this->m_homeId, this->m_nodeId, (*find).get<std::string>() );
						} else {
							throw WebServer::ResourceException( 400, "Hardware.Invalid.Name", "The supplied name is invalid." );
						}
					}

					// Then all pushed settings are processed. The settings should be pushed to the server in
					// the same format as they are received.
					find = input_.find( "settings" );
					if ( find != input_.end() ) {
						if ( (*find).is_array() ) {

							for ( auto settingIt = (*find).begin(); settingIt != (*find).end(); settingIt++ ) {
								auto setting = (*settingIt);
								
								// Only process this setting if it exists in the configuration.
								if (
									setting.find( "name" ) != setting.end()
									&& setting.find( "value" ) != setting.end()
									&& this->m_configuration.find( setting["name"].get<std::string>() ) != this->m_configuration.end()
								) {
									bool success = true;
									auto reference = setting["name"].get<std::string>();
									auto& config = this->m_configuration[reference];
									
									if ( config["readonly"].get<bool>() ) {
										continue;
									}

									ValueID valueId( this->m_homeId, std::stoull( reference ) );
									ValueID::ValueType type = valueId.GetType();
									
									if ( type == ValueID::ValueType_Decimal ) {
										success = success && setting["value"].get<float>() <= config["max"].get<float>();
										success = success && setting["value"].get<float>() >= config["min"].get<float>();
										success = success && Manager::Get()->SetValue( valueId, setting["value"].get<float>() );
										if ( success ) {
											config["value"] = setting["value"].get<float>();
										}
									} else if ( type == ValueID::ValueType_Bool ) {
										success = success && Manager::Get()->SetValue( valueId, setting["value"].get<bool>() );
										if ( success ) {
											config["value"] = setting["value"].get<bool>();
										}
									} else if ( type == ValueID::ValueType_Byte ) {
										success = success && setting["value"].get<unsigned int>() <= config["max"].get<unsigned int>();
										success = success && setting["value"].get<unsigned int>() >= config["min"].get<unsigned int>();
										success = success && Manager::Get()->SetValue( valueId, setting["value"].get<uint8>() );
										if ( success ) {
											config["value"] = setting["value"].get<uint8>();
										}
									} else if ( type == ValueID::ValueType_Short ) {
										success = success && setting["value"].get<int>() <= config["max"].get<int>();
										success = success && setting["value"].get<int>() >= config["min"].get<int>();
										success = success && Manager::Get()->SetValue( valueId, setting["value"].get<int16>() );
										if ( success ) {
											config["value"] = setting["value"].get<int16>();
										}
									} else if ( type == ValueID::ValueType_Int ) {
										success = success && setting["value"].get<int>() <= config["max"].get<int>();
										success = success && setting["value"].get<int>() >= config["min"].get<int>();
										success = success && Manager::Get()->SetValue( valueId, setting["value"].get<int32>() );
										if ( success ) {
											config["value"] = setting["value"].get<int32>();
										}
									} else if ( type == ValueID::ValueType_List ) {
										bool exists = false;
										for ( auto itemsIt = config["options"].begin(); itemsIt != config["options"].end(); itemsIt++ ) {
											if ( (*itemsIt)["value"] == setting["value"].get<std::string>() ) {
												exists = true;
												break;
											}
										}
										success = success && exists;
										success = success && Manager::Get()->SetValueListSelection( valueId, setting["value"].get<std::string>() );
										if ( success ) {
											config["value"] = setting["value"].get<std::string>();
										}
									} else if ( type == ValueID::ValueType_String ) {
										success = success && Manager::Get()->SetValue( valueId, setting["value"].get<std::string>() );
										if ( success ) {
											config["value"] = setting["value"].get<std::string>();
										}
									}
									
									if ( ! success ) {
										unsigned int index = valueId.GetIndex();
										throw WebServer::ResourceException( 400, "Hardware.Invalid.Settings", "Invalid value for parameter " + std::to_string( index ) + "." );
									}
								}
							}
						} else {
							throw WebServer::ResourceException( 400, "Hardware.Invalid.Settings", "The supplied settings are invalid." );
						}

						output_["code"] = 200;
					}
				} else {
					g_logger->log( Logger::LogLevel::ERROR, this, "Controller busy." );
					throw WebServer::ResourceException( 423, "Hardware.Busy", "The hardware is busy." );
				}
			} )
		} );
	};

}; // namespace micasa
