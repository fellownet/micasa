#include <chrono>

#include "OpenZWaveNode.h"

#include "../Logger.h"
#include "../Controller.h"
#include "../WebServer.h"

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

#define COMMAND_CLASS_NO_OPERATION				0x00

#define COMMAND_CLASS_BASIC						0x20
#define COMMAND_CLASS_CONTROLLER_REPLICATION	0x21
#define COMMAND_CLASS_APPLICATION_STATUS			0x22
#define COMMAND_CLASS_ZIP_SERVICES				0x23
#define COMMAND_CLASS_ZIP_SERVER					0x24
#define COMMAND_CLASS_SWITCH_BINARY				0x25
#define COMMAND_CLASS_SWITCH_MULTILEVEL			0x26
#define COMMAND_CLASS_SWITCH_ALL					0x27
#define COMMAND_CLASS_SWITCH_TOGGLE_BINARY		0x28
#define COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL	0x29
#define COMMAND_CLASS_CHIMNEY_FAN				0x2A
#define COMMAND_CLASS_SCENE_ACTIVATION			0x2B
#define COMMAND_CLASS_SCENE_ACTUATOR_CONF		0x2C
#define COMMAND_CLASS_SCENE_CONTROLLER_CONF		0x2D
#define COMMAND_CLASS_ZIP_CLIENT					0x2E
#define COMMAND_CLASS_ZIP_ADV_SERVICES			0x2F
#define COMMAND_CLASS_SENSOR_BINARY				0x30
#define COMMAND_CLASS_SENSOR_MULTILEVEL			0x31
#define COMMAND_CLASS_METER						0x32
#define COMMAND_CLASS_COLOR_CONTROL				0x33
#define COMMAND_CLASS_ZIP_ADV_CLIENT				0x34
#define COMMAND_CLASS_METER_PULSE				0x35
#define COMMAND_CLASS_THERMOSTAT_HEATING			0x38

#define COMMAND_CLASS_METER_TBL_CONFIG			0x3C
#define COMMAND_CLASS_METER_TBL_MONITOR			0x3D
#define COMMAND_CLASS_METER_TBL_PUSH				0x3E


#define COMMAND_CLASS_THERMOSTAT_MODE			0x40
#define COMMAND_CLASS_THERMOSTAT_OPERATING_STATE	0x42
#define COMMAND_CLASS_THERMOSTAT_SETPOINT		0x43
#define COMMAND_CLASS_THERMOSTAT_FAN_MODE		0x44
#define COMMAND_CLASS_THERMOSTAT_FAN_STATE		0x45
#define COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE	0x46
#define COMMAND_CLASS_THERMOSTAT_SETBACK		0x47

#define COMMAND_CLASS_BASIC_WINDOW_COVERING		0x50
#define COMMAND_CLASS_MTP_WINDOW_COVERING		0x51

#define COMMAND_CLASS_CRC16_ENCAP				0x56

#define COMMAND_CLASS_DEVICE_RESET_LOCALLY		0x5A
#define COMMAND_CLASS_CENTRAL_SCENE				0x5B

#define COMMAND_CLASS_ZWAVE_PLUS_INFO			0x5E

#define COMMAND_CLASS_MULTI_INSTANCE			0x60
#define COMMAND_CLASS_DOOR_LOCK					0x62
#define COMMAND_CLASS_USER_CODE					0x63

#define COMMAND_CLASS_CONFIGURATION				0x70
#define COMMAND_CLASS_ALARM						0x71
#define COMMAND_CLASS_MANUFACTURER_SPECIFIC		0x72
#define COMMAND_CLASS_POWERLEVEL				0x73
#define COMMAND_CLASS_PROTECTION				0x75
#define COMMAND_CLASS_LOCK						0x76
#define COMMAND_CLASS_NODE_NAMING				0x77
#define COMMAND_CLASS_FIRMWARE_UPDATE_MD		0x7A
#define COMMAND_CLASS_GROUPING_NAME				0x7B
#define COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE	0x7C
#define COMMAND_CLASS_REMOTE_ASSOCIATION		0x7D
#define COMMAND_CLASS_BATTERY					0x80
#define COMMAND_CLASS_CLOCK						0x81
#define COMMAND_CLASS_HAIL						0x82
#define COMMAND_CLASS_WAKE_UP					0x84
#define COMMAND_CLASS_ASSOCIATION				0x85
#define COMMAND_CLASS_VERSION					0x86
#define COMMAND_CLASS_INDICATOR					0x87
#define COMMAND_CLASS_PROPRIETARY				0x88
#define COMMAND_CLASS_LANGUAGE					0x89
#define COMMAND_CLASS_TIME						0x8A
#define COMMAND_CLASS_TIME_PARAMETERS			0x8B
#define COMMAND_CLASS_GEOGRAPHIC_LOCATION		0x8C
#define COMMAND_CLASS_COMPOSITE					0x8D
#define COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION	0x8E
#define COMMAND_CLASS_MULTI_CMD					0x8F
#define COMMAND_CLASS_ENERGY_PRODUCTION			0x90
#define COMMAND_CLASS_MANUFACTURER_PROPRIETARY	0x91
#define COMMAND_CLASS_SCREEN_MD					0x92
#define COMMAND_CLASS_SCREEN_ATTRIBUTES			0x93
#define COMMAND_CLASS_SIMPLE_AV_CONTROL			0x94
#define COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD	0x95
#define COMMAND_CLASS_AV_RENDERER_STATUS		0x96
#define COMMAND_CLASS_AV_CONTENT_SEARCH_MD		0x97
#define COMMAND_CLASS_SECURITY					0x98
#define COMMAND_CLASS_AV_TAGGING_MD				0x99
#define COMMAND_CLASS_IP_CONFIGURATION			0x9A
#define COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION	0x9B
#define COMMAND_CLASS_SENSOR_ALARM				0x9C
#define COMMAND_CLASS_SILENCE_ALARM				0x9D
#define COMMAND_CLASS_SENSOR_CONFIGURATION		0x9E
#define COMMAND_CLASS_NON_INTEROPERABLE			0xF0

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	const std::map<std::string, unsigned int> OpenZWaveNode::UnitMapping = {
		{ "Energy", Counter::Unit::KILOWATTHOUR },
		{ "Power", Level::Unit::WATT },
		{ "Voltage", Level::Unit::VOLT },
		{ "Current", Level::Unit::AMPERES },
		{ "Power Factor", Level::Unit::POWER_FACTOR },
		{ "Gas", Counter::Unit::M3 },
		{ "Water", Counter::Unit::M3 },
		{ "Temperature", Level::Unit::CELCIUS },
		{ "Luminance", Level::Unit::LUX },
		{ "Relative Humidity", Level::Unit::GENERIC },
		{ "Ultraviolet", Level::Unit::GENERIC },
		{ "Velocity", Level::Unit::GENERIC },
		{ "Barometric Pressure", Level::Unit::GENERIC },
		{ "Dew Point", Level::Unit::GENERIC },
		{ "CO2 Level", Level::Unit::GENERIC },
		{ "Moisture", Level::Unit::GENERIC }
	};

	OpenZWaveNode::OpenZWaveNode( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		// The settings for openzwave nodes are not mereley stored in the database, they need to be pushed to
		// the node too.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ), 99, // just prior to the generic callback handler
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				try {
					std::shared_ptr<OpenZWave> parent = std::static_pointer_cast<OpenZWave>( this->m_parent );

					// Obtain a lock on the manager of the parent openzwave transaction.
					if ( parent->m_notificationMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
						std::lock_guard<std::timed_mutex> lock( parent->m_notificationMutex, std::adopt_lock );

						// Set the node name so it's persistant.
						if ( input_.find( "name" ) != input_.end() ) {
							::OpenZWave::Manager::Get()->SetNodeName( parent->m_homeId, std::stoull( this->m_settings->get( "node_id" ) ), input_["name"].get<std::string>() );
						}

						if (
							input_.find( "settings" ) != input_.end()
							&& input_["settings"].is_array()
						) {
							bool success = true;
							for ( auto settingIt = input_["settings"].begin(); settingIt != input_["settings"].end(); settingIt++ ) {
								auto setting = (*settingIt);
								if (
									setting.find( "name" ) != setting.end()
									&& setting.find( "value" ) != setting.end()
									&& this->m_configuration.find( setting["name"].get<std::string>() ) != this->m_configuration.end()
								) {
								
									// This inner block resides in a separate try/catch block to make sure that all settings
									// are processed instead of stopping after the first error.
									try {
								
										auto reference = setting["name"].get<std::string>();
										auto& config = this->m_configuration[reference];
										
										std::shared_ptr<OpenZWave> parent = std::static_pointer_cast<OpenZWave>( this->m_parent );
										::OpenZWave::ValueID valueId( parent->m_homeId, std::stoull( reference ) );

										// TODO check if value is different from the one in m_conf, then it is most likely it has
										// changed by the client and only then update the device.
										// TODO settings also need to be stored in the database because after a restart the settings
										// will hold the defaults until a device wakes up.

										::OpenZWave::ValueID::ValueType type = valueId.GetType();
										if ( type == ::OpenZWave::ValueID::ValueType_Decimal ) {
											success == success && ::OpenZWave::Manager::Get()->SetValue( valueId, setting["value"].get<float>() );
											if ( success ) {
												config["value"] = setting["value"].get<float>();
											}
										} else if ( type == ::OpenZWave::ValueID::ValueType_Bool ) {
											success == success && ::OpenZWave::Manager::Get()->SetValue( valueId, setting["value"].get<bool>() );
											if ( success ) {
												config["value"] = setting["value"].get<bool>();
											}
										} else if ( type == ::OpenZWave::ValueID::ValueType_Byte ) {
											success == success && ::OpenZWave::Manager::Get()->SetValue( valueId, setting["value"].get<uint8>() );
											if ( success ) {
												config["value"] = setting["value"].get<uint8>();
											}
										} else if ( type == ::OpenZWave::ValueID::ValueType_Short ) {
											success == success && ::OpenZWave::Manager::Get()->SetValue( valueId, setting["value"].get<int16>() );
											if ( success ) {
												config["value"] = setting["value"].get<int16>();
											}
										} else if ( type == ::OpenZWave::ValueID::ValueType_Int ) {
											success == success && ::OpenZWave::Manager::Get()->SetValue( valueId, setting["value"].get<int32>() );
											if ( success ) {
												config["value"] = setting["value"].get<int32>();
											}
										} else if ( type == ::OpenZWave::ValueID::ValueType_List ) {
											success == success && ::OpenZWave::Manager::Get()->SetValueListSelection( valueId, setting["value"].get<std::string>() );
											if ( success ) {
												config["value"] = setting["value"].get<std::string>();
											}
										} else if ( type == ::OpenZWave::ValueID::ValueType_String ) {
											success == success && ::OpenZWave::Manager::Get()->SetValue( valueId, setting["value"].get<std::string>() );
											if ( success ) {
												config["value"] = setting["value"].get<std::string>();
											}
										}
									} catch( const std::exception& ex_ ) {
										success = false;
										output_["message"] = "Error while updating hardware (" + std::string( ex_.what() ) + ").";
									}
								}
							}
							
							if ( success ) {
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
								if ( output_.find( "message" ) == output_.end() ) {
									output_["message"] = "Unable to update hardware.";
								}
								code_ = 400; // bad request > causes all other callbacks to not get called
							}
						}
					} else {
						g_logger->log( Logger::LogLevel::ERROR, this, "Controller busy." );
						output_["result"] = "ERROR";
						output_["message"] = "Hardware busy.";
						code_ = 400; // bad request > causes all other callbacks to not get called
					}
				} catch( ... ) {
					output_["result"] = "ERROR";
					output_["message"] = "Unable to update hardware.";
					code_ = 400; // bad request > causes all other callbacks to not get called
				}
			} )
		} ) ) );
	};

	void OpenZWaveNode::start() {
#ifdef _DEBUG
		assert( this->m_settings->contains( { "home_id", "node_id" } ) && "OpenZWaveNode should be declared with home_id and node_id." );
#endif // _DEBUG

		// TODO when this hardware is started and stopped while the openzwave parent hardware keeps running,
		// the state remains initializing > fix. Check the parent initializing state. If it is still initializing
		// do nothing, otherwise query the node our self. This still might overlap, so additional tweaking is
		// probably necessary.
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		this->_installResourceHandlers();
		Hardware::start();
	};

	void OpenZWaveNode::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		g_webServer->removeResourceCallback( "openzwavenode-" + std::to_string( this->m_id ) );
		Hardware::stop();
	};

	std::string OpenZWaveNode::getLabel() const {
		return this->m_settings->get( "label", "OpenZWave Node" );
	};

	json OpenZWaveNode::getJson( bool full_ ) const {
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

	bool OpenZWaveNode::updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		apply_ = false;

		std::shared_ptr<OpenZWave> parent = std::static_pointer_cast<OpenZWave>( this->m_parent );

		// Obtain a lock on the manager of the parent openzwave transaction.
		if ( parent->m_notificationMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
			std::lock_guard<std::timed_mutex> lock( parent->m_notificationMutex, std::adopt_lock );

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
			::OpenZWave::ValueID valueId( parent->m_homeId, std::stoull( device_->getReference() ) );
			switch( valueId.GetCommandClassId() ) {
				case COMMAND_CLASS_SWITCH_BINARY: {
					if (
						valueId.GetType() == ::OpenZWave::ValueID::ValueType_Bool
						&& device_->getType() == Device::Type::SWITCH
					) {
						// TODO differentiate between blinds, switches etc (like open close on of etc).
						if ( this->_queuePendingUpdate( device_->getReference(), source_, OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC, OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC ) ) {
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
							bool value = ( device->getValueOption() == Switch::Option::ON ) ? true : false;
							return ::OpenZWave::Manager::Get()->SetValue( valueId, value );
						} else {
							g_logger->log( Logger::LogLevel::ERROR, this, "Node busy." );
							return false;
						}
					}
				}
				case COMMAND_CLASS_SWITCH_MULTILEVEL: {
					if (
						valueId.GetType() == ::OpenZWave::ValueID::ValueType_Byte
						&& device_->getType() == Device::Type::LEVEL
					) {
						if ( this->_queuePendingUpdate( device_->getReference(), source_, OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC, OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC ) ) {
							std::shared_ptr<Level> device = std::static_pointer_cast<Level>( device_ );
							return ::OpenZWave::Manager::Get()->SetValue( valueId, uint8( device->getValue() ) );
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

	void OpenZWaveNode::_handleNotification( const ::OpenZWave::Notification* notification_ ) {

		// This method is proxied from the OpenZWave class which still holds the OpenZWave manager lock. This
		// is required when processing notifications.

		unsigned int homeId = notification_->GetHomeId();
		unsigned char nodeId = notification_->GetNodeId();
#ifdef _DEBUG
		assert( this->m_settings->get<unsigned int>( "home_id" ) == homeId && "OpenZWaveNode home_id should be correct." );
		assert( this->m_settings->get<unsigned int>( "node_id" ) == nodeId && "OpenZWaveNode home_id should be correct." );
#endif // _DEBUG

		switch( notification_->GetType() ) {

			case ::OpenZWave::Notification::Type_EssentialNodeQueriesComplete:
			case ::OpenZWave::Notification::Type_NodeQueriesComplete: {

				// This is the point after which the node can accept commands.
				if ( this->getState() == Hardware::State::INIT ) {
					g_logger->log( Logger::LogLevel::NORMAL, this, "Node ready." );
					this->setState( Hardware::State::READY );
				}
			}

			case ::OpenZWave::Notification::Type_ValueAdded: {
				::OpenZWave::ValueID valueId = notification_->GetValueID();
				this->_processValue( valueId, Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE );
				this->_updateNodeInfo( homeId, nodeId );
				break;
			}

			case ::OpenZWave::Notification::Type_ValueChanged: {
				::OpenZWave::ValueID valueId = notification_->GetValueID();
				this->_processValue( valueId, Device::UpdateSource::HARDWARE );
				break;
			}

			case ::OpenZWave::Notification::Type_ValueRemoved: {
				// TODO remove device? an what about history and trends that the user might want to keep?
				break;
			}

			case ::OpenZWave::Notification::Type_Notification: {
				switch( notification_->GetNotification() ) {
					case ::OpenZWave::Notification::Code_Alive: {
						if ( this->getState() == Hardware::State::FAILED ) {
							g_logger->log( Logger::LogLevel::NORMAL, this, "Node is alive again." );
						}
						this->_updateNodeInfo( homeId, nodeId );
						this->setState( Hardware::State::READY );
						break;
					}
					case ::OpenZWave::Notification::Code_Dead: {
						g_logger->log( Logger::LogLevel::ERROR, this, "Node is dead." );
						this->setState( Hardware::State::FAILED );
						break;
					}
					case ::OpenZWave::Notification::Code_Awake: {
						if ( this->getState() == Hardware::State::SLEEPING ) {
							g_logger->log( Logger::LogLevel::NORMAL, this, "Node is awake again." );
						}
						this->setState( Hardware::State::READY );
						this->_updateNodeInfo( homeId, nodeId );
						break;
					}
					case ::OpenZWave::Notification::Code_Sleep: {
						g_logger->log( Logger::LogLevel::NORMAL, this, "Node is asleep." );
						this->setState( Hardware::State::SLEEPING );
						break;
					}
					case ::OpenZWave::Notification::Code_Timeout: {
						g_logger->log( Logger::LogLevel::WARNING, this, "Node timeout." );
						break;
					}
				}
				break;
			}

			case ::OpenZWave::Notification::Type_NodeEvent: {
				break;
			}

			case ::OpenZWave::Notification::Type_NodeNaming: {
				this->_updateNodeInfo( homeId, nodeId );
				break;
			}

			default: {
				break;
			}
		}
	};

	void OpenZWaveNode::_processValue( const ::OpenZWave::ValueID& valueId_, unsigned int source_ ) {
		std::string label = ::OpenZWave::Manager::Get()->GetValueLabel( valueId_ );
		std::string reference = std::to_string( valueId_.GetId() );
		std::string units = ::OpenZWave::Manager::Get()->GetValueUnits( valueId_ );

		// Some values are not going to be processed ever and can be filtered out beforehand.
		if (
			"Exporting" == label
			|| "Interval" == label
			|| "Previous Reading" == label
			|| "Library Version" == label
			|| "Protocol Version" == label
			|| "Application Version" == label
			|| "Previous Reading" == label
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
				// TODO if a switch comes too soon after a manual switch (not from hardware) ignore- or revert it.
				// TODO this prevents having to code javascript to ignore switches from happing right after a PIR
				// instruction.
				unsigned int allowedUpdateSources = Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE;
				if ( commandClass == COMMAND_CLASS_SWITCH_BINARY ) {
					allowedUpdateSources |= Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API;

					// If there's an update mutex available we need to make sure that it is properly notified of the
					// execution of the update.
					source_ |= this->_releasePendingUpdate( reference );
				}
				bool boolValue = false;
				unsigned char byteValue = 0;
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Bool
					&& false != ::OpenZWave::Manager::Get()->GetValueAsBool( valueId_, &boolValue )
				) {
					// TODO differentiate between blinds, switches etc (like open close on of etc).
					auto device = this->_declareDevice<Switch>( reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( allowedUpdateSources ) }
					} );
					device->updateValue( source_, boolValue ? Switch::Option::ON : Switch::Option::OFF );
					if ( "Unknown" != label ) {
						device->setLabel( label );
					}
				}
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Byte
					&& false != ::OpenZWave::Manager::Get()->GetValueAsByte( valueId_, &byteValue )
				) {
					// TODO differentiate between blinds, switches etc (like open close on of etc).
					auto device = this->_declareDevice<Switch>( reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( allowedUpdateSources ) }
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
					unsigned int allowedUpdateSources = Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API;

					// If there's an update mutex available we need to make sure that it is properly notified of the
					// execution of the update.
					source_ |= this->_releasePendingUpdate( reference );

					unsigned char byteValue = 0;

					if (
						valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Byte
						&& false != ::OpenZWave::Manager::Get()->GetValueAsByte( valueId_, &byteValue )
					) {
						auto device = this->_declareDevice<Level>( reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( allowedUpdateSources ) },
							{ DEVICE_SETTING_UNITS, std::to_string( Level::Unit::PERCENT ) }
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
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Decimal
					&& false != ::OpenZWave::Manager::Get()->GetValueAsFloat( valueId_, &floatValue )
				) {
					unsigned int unit = 1;
					if ( OpenZWaveNode::UnitMapping.find( label ) != OpenZWaveNode::UnitMapping.end() ) {
						unit = OpenZWaveNode::UnitMapping.at( label );
						if (
							label == "Temperature"
							&& units == "F"
						) {
							unit = Level::Unit::FAHRENHEIT;
						}
					}
					
					// TODO check units to see if any multiplication needs to take place (from watt kwh)?
				
					if (
						"Energy" == label
						|| "Gas" == label
						|| "Water" == label
					) {
						auto device = this->_declareDevice<Counter>( reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
							{ DEVICE_SETTING_UNITS, std::to_string( unit ) }
						} );
						device->updateValue( source_, floatValue );
					} else {
						auto device = this->_declareDevice<Level>( reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
							{ DEVICE_SETTING_UNITS, std::to_string( unit ) }
						} );
						device->updateValue( source_, floatValue );
					}
				}
				break;
			}

			case COMMAND_CLASS_BATTERY: {
				unsigned char byteValue = 0;
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Byte
					&& false != ::OpenZWave::Manager::Get()->GetValueAsByte( valueId_, &byteValue )
				) {
					auto device = this->_declareDevice<Level>( reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
						{ DEVICE_SETTING_UNITS, std::to_string( (unsigned int)Level::Unit::PERCENT ) }
					} );
					device->updateValue( source_, (unsigned int)byteValue );
				}
				break;
			}

			//case COMMAND_CLASS_PROTECTION: // TODO can be used to lock or unlock configuration?
			case COMMAND_CLASS_WAKE_UP:
			case COMMAND_CLASS_CONFIGURATION: {
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
					{ "description", ::OpenZWave::Manager::Get()->GetValueHelp( valueId_ ) },
					{ "class", "advanced" }
				};

				::OpenZWave::ValueID::ValueType type = valueId_.GetType();
				if ( type == ::OpenZWave::ValueID::ValueType_Decimal ) {
					setting["type"] = "double";
					setting["min"] = ::OpenZWave::Manager::Get()->GetValueMin( valueId_ );
					setting["max"] = ::OpenZWave::Manager::Get()->GetValueMax( valueId_ );
					float floatValue = 0;
					::OpenZWave::Manager::Get()->GetValueAsFloat( valueId_, &floatValue );
					setting["value"] = floatValue;
				} else if ( type == ::OpenZWave::ValueID::ValueType_Bool ) {
					setting["type"] = "boolean";
					bool boolValue = 0;
					::OpenZWave::Manager::Get()->GetValueAsBool( valueId_, &boolValue );
					setting["value"] = boolValue;
				} else if ( type == ::OpenZWave::ValueID::ValueType_Byte ) {
					setting["type"] = "byte";
					setting["min"] = ::OpenZWave::Manager::Get()->GetValueMin( valueId_ );
					setting["max"] = ::OpenZWave::Manager::Get()->GetValueMax( valueId_ );
					unsigned char byteValue = 0;
					::OpenZWave::Manager::Get()->GetValueAsByte( valueId_, &byteValue );
					setting["value"] = byteValue;
				} else if ( type == ::OpenZWave::ValueID::ValueType_Short ) {
					setting["type"] = "short";
					setting["min"] = ::OpenZWave::Manager::Get()->GetValueMin( valueId_ );
					setting["max"] = ::OpenZWave::Manager::Get()->GetValueMax( valueId_ );
					short shortValue = 0;
					::OpenZWave::Manager::Get()->GetValueAsShort( valueId_, &shortValue );
					setting["value"] = shortValue;
				} else if ( type == ::OpenZWave::ValueID::ValueType_Int ) {
					setting["type"] = "int";
					setting["min"] = ::OpenZWave::Manager::Get()->GetValueMin( valueId_ );
					setting["max"] = ::OpenZWave::Manager::Get()->GetValueMax( valueId_ );
					int intValue = 0;
					::OpenZWave::Manager::Get()->GetValueAsInt( valueId_, &intValue );
					setting["value"] = intValue;
				} else if ( type == ::OpenZWave::ValueID::ValueType_Button ) {
					// Perform action, such as reset to factory defaults.
					// TODO these should actually be action resources directly, but how to name the resource?
					//setting["type"] = "button";
					break; // skip this setting for now > unsupported
				} else if ( type == ::OpenZWave::ValueID::ValueType_List ) {
					setting["type"] = "list";
					std::string listValue;
					::OpenZWave::Manager::Get()->GetValueListSelection( valueId_, &listValue );
					setting["value"] = listValue;

					setting["options"] = json::array();
					std::vector<std::string> items;
					::OpenZWave::Manager::Get()->GetValueListItems( valueId_, &items );
					for ( auto itemsIt = items.begin(); itemsIt != items.end(); itemsIt++ ) {
						json option = json::object();
						option["value"] = (*itemsIt);
						option["label"] = (*itemsIt);
						setting["options"] += option;
					}
				} else if ( type == ::OpenZWave::ValueID::ValueType_String ) {
					setting["type"] = "string";
					std::string stringValue;
					::OpenZWave::Manager::Get()->GetValueAsString( valueId_, &stringValue );
					setting["value"] = stringValue;
				}

				std::unique_lock<std::mutex> lock( this->m_configurationMutex );
				this->m_configuration[reference] = setting;
				lock.unlock();
				break;
			}

			case COMMAND_CLASS_ALARM:
			case COMMAND_CLASS_SENSOR_ALARM: {
				// https://github.com/OpenZWave/open-zwave/wiki/Alarm-Command-Class
				/*
				unsigned char byteValue = 0;
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Byte
					&& false != ::OpenZWave::Manager::Get()->GetValueAsByte( valueId_, &byteValue )
				) {
					std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->_declareDevice( Device::Type::SWITCH, reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) }
					} ) );
					device->updateValue( source_, byteValue ? Switch::Option::ON : Switch::Option::OFF );
				}
				*/
				break;
			}

			default: {
				g_logger->logr( Logger::LogLevel::ERROR, this, "Unhandled command class %#010x (%s).", commandClass, label.c_str() );
				break;
			}
		}
	};

	void OpenZWaveNode::_updateNodeInfo( const unsigned int& homeId_, const unsigned char& nodeId_ ) {
		std::string manufacturer = ::OpenZWave::Manager::Get()->GetNodeManufacturerName( homeId_, nodeId_ );
		std::string product = ::OpenZWave::Manager::Get()->GetNodeProductName( homeId_, nodeId_ );
		std::string nodeName = ::OpenZWave::Manager::Get()->GetNodeName( homeId_, nodeId_ );
		if ( ! manufacturer.empty() ) {
			this->m_settings->put( "label", manufacturer + " " + product );
		}
		if (
			! nodeName.empty()
			&& this->m_settings->get( "name", "" ).empty()
		) {
			this->m_settings->put( "name", nodeName );
		}
	};

	void OpenZWaveNode::_installResourceHandlers() const {

		// Remove previously installed callbacks.
		g_webServer->removeResourceCallback( "openzwavenode-" + std::to_string( this->m_id ) );
	
		// Add resource handlers for network heal.
		// http://www.openzwave.com/knowledge-base/deadnode
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"openzwavenode-" + std::to_string( this->m_id ),
			"api/hardware/" + std::to_string( this->m_id ) + "/heal", 101,
			WebServer::Method::PUT,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				std::shared_ptr<OpenZWave> parent = std::static_pointer_cast<OpenZWave>( this->m_parent );
				if ( parent->m_notificationMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
					std::lock_guard<std::timed_mutex> lock( parent->m_notificationMutex, std::adopt_lock );
					if ( parent->getState() == Hardware::State::READY ) {
						::OpenZWave::Manager::Get()->HealNetworkNode( parent->m_homeId, std::stoull( this->m_settings->get( "node_id" ) ), true );
						output_["result"] = "OK";
						g_logger->log( Logger::LogLevel::NORMAL, this, "Node heal initiated." );
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
	};

}; // namespace micasa
