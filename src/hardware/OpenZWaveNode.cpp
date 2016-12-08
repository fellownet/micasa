#ifdef WITH_OPENZWAVE

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

	bool OpenZWaveNode::updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		apply_ = false;

		std::shared_ptr<OpenZWave> parent = std::static_pointer_cast<OpenZWave>( this->m_parent );
		if ( parent->m_managerMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC ) ) ) {
			
			// Obtain the lock created above. This allows for return statements without explicitly releasing the lock, this
			// is done in the lock_guard destructor.
			std::lock_guard<std::timed_mutex> lock( parent->m_managerMutex, std::adopt_lock );
			
			if ( parent->m_controllerState == OpenZWave::State::IDLE ) {
			
				// If the node has been reported dead do not try to send the command.
				if ( this->m_dead ) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Node is dead." );
					return false;
				}
				
				// TODO Create lock and release when the node has set the value? Must be temporary though, so better is to
				// sleep and use a notifiaction variable such as in worker class?
				// TODO also remember the source so we can provide SCRIPT instead of hardware.
				
				// Reconstruct the value id which is needed to send a command to a node.
				::OpenZWave::ValueID valueId( parent->m_homeId, std::stoull( device_->getReference() ) );
				switch( valueId.GetCommandClassId() ) {
					case COMMAND_CLASS_SWITCH_BINARY: {
						if (
							valueId.GetType() == ::OpenZWave::ValueID::ValueType_Bool
							&& device_->getType() == Device::Type::SWITCH
						) {
							// TODO differentiate between blinds, switches etc (like open close on of etc).
							if ( this->_queuePendingUpdate( valueId.GetId(), source_ ) ) {
								std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
								bool value = ( device->getValueOption() == Switch::Option::ON ) ? true : false;
								return ::OpenZWave::Manager::Get()->SetValue( valueId, value );
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
		
		} else {
			g_logger->log( Logger::LogLevel::ERROR, this, "Controller manager busy." );
			return false;
		}

		return true;
	};
	
	
	void OpenZWaveNode::_handleNotification( const ::OpenZWave::Notification* notification_ ) {
		// This method is proxied from the OpenZWave class which still holds the OpenZWave manager lock. This
		// is required when processing notifications.

		unsigned int homeId = notification_->GetHomeId();
		unsigned char nodeId = notification_->GetNodeId();

		g_logger->log( Logger::LogLevel::VERBOSE, this, notification_->GetAsString() );

		switch( notification_->GetType() ) {
			case ::OpenZWave::Notification::Type_ValueAdded: {
				::OpenZWave::ValueID valueId = notification_->GetValueID();
				this->_processValue( valueId, Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE );

				// Also try to update the label here too, in addition to the nodeNaming notification.
				std::string manufacturer = ::OpenZWave::Manager::Get()->GetNodeManufacturerName( homeId, nodeId );
				std::string product = ::OpenZWave::Manager::Get()->GetNodeProductName( homeId, nodeId );
				if ( ! manufacturer.empty() ) {
					this->setLabel( manufacturer + " " + product );
				}
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
						if ( this->m_dead ) {
							g_logger->log( Logger::LogLevel::NORMAL, this, "Node is alive again." );
							this->m_dead = false;
						}
						break;
					}
					case ::OpenZWave::Notification::Code_Dead: {
						g_logger->log( Logger::LogLevel::ERROR, this, "Node is dead." );
						this->m_dead = true;
						break;
					}
						
				}
				break;
			}
				
			case ::OpenZWave::Notification::Type_NodeEvent: {
				std::cout << "NODE EVENT, UNHANDLED?\n";
				break;
			}

			case ::OpenZWave::Notification::Type_NodeNaming: {
				std::string manufacturer = ::OpenZWave::Manager::Get()->GetNodeManufacturerName( homeId, nodeId );
				std::string product = ::OpenZWave::Manager::Get()->GetNodeProductName( homeId, nodeId );
				if ( ! manufacturer.empty() ) {
					this->setLabel( manufacturer + " " + product );
				}
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

		// Some values are not going to be processed ever and can be filtered out beforehand.
		if (
			"Exporting" == label
			|| "Interval" == label
			|| "Previous Reading" == label
		) {
			return;
		}

		// If there's an update mutex available we need to make sure that it is properly notified of the
		// execution of the update.
		std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( valueId_.GetId() );
		if ( search != this->m_pendingUpdates.end() ) {
			auto pendingUpdate = search->second;
			std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
			pendingUpdate->done = true;
			notifyLock.unlock();
			pendingUpdate->condition.notify_all();
			source_ |= pendingUpdate->source;
		}
		pendingUpdatesLock.unlock();

		// Process all other values by command class.
		unsigned int commandClass = valueId_.GetCommandClassId();
		switch( commandClass ) {
				
			case COMMAND_CLASS_BASIC: {
				// https://github.com/OpenZWave/open-zwave/wiki/Basic-Command-Class
				// Conclusion; try to use devices that send other cc values aswell as the basic set. For instance,
				// the Aeotec ZW089 Recessed Door Sensor Gen5 defaults to only sending basic messages BUT can be
				// configured to also send binary reports.
				break;
			}
				
			case COMMAND_CLASS_SWITCH_BINARY:
			case COMMAND_CLASS_SENSOR_BINARY: {
				unsigned int updateSources = Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE;
				if ( commandClass == COMMAND_CLASS_SWITCH_BINARY ) {
					updateSources |= Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API;
				}
				bool boolValue = false;
				unsigned char byteValue = 0;
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Bool
					&& false != ::OpenZWave::Manager::Get()->GetValueAsBool( valueId_, &boolValue )
				) {
					// TODO differentiate between blinds, switches etc (like open close on of etc).
					std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->_declareDevice( Device::Type::SWITCH, reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( updateSources ) }
					} ) );
					device->updateValue( source_, boolValue ? Switch::Option::ON : Switch::Option::OFF );
				}
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Byte
					&& false != ::OpenZWave::Manager::Get()->GetValueAsByte( valueId_, &byteValue )
				) {
					// TODO differentiate between blinds, switches etc (like open close on of etc).
					std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->_declareDevice( Device::Type::SWITCH, reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( updateSources ) }
					} ) );
					
					device->updateValue( source_, byteValue ? Switch::Option::ON : Switch::Option::OFF );
				}
				break;
			}
			
			case COMMAND_CLASS_METER: {
				
				float floatValue = 0;
				if (
					valueId_.GetType() == ::OpenZWave::ValueID::ValueType_Decimal
					&& false != ::OpenZWave::Manager::Get()->GetValueAsFloat( valueId_, &floatValue )
				) {
					if (
						"Energy" == label
						|| "Gas" == label
						|| "Water" == label
					) {
						std::shared_ptr<Counter> device = std::static_pointer_cast<Counter>( this->_declareDevice( Device::Type::COUNTER, reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) }
						} ) );
						device->updateValue( source_, floatValue );
					} else {
						std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, reference, label, {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) }
						} ) );
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
					std::shared_ptr<Level> device = std::static_pointer_cast<Level>( this->_declareDevice( Device::Type::LEVEL, reference, label, {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) }
					} ) );
					device->updateValue( source_, (unsigned int)byteValue );
				}
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
				
			case COMMAND_CLASS_CONFIGURATION: {
				// ignore for now
				break;
			}
				
			default: {
				//g_logger->logr( Logger::LogLevel::ERROR, this, "Unhandled command class %#010x (%s).", commandClass, label.c_str() );
				// not implemented (yet?)
				break;
			}
		}
	};

	const bool OpenZWaveNode::_queuePendingUpdate( const unsigned long long valueId_, const unsigned int source_ ) {

		// See if there's already a pending update present, in which case we need to use it to start locking.
		std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( valueId_ );
		if ( search == this->m_pendingUpdates.end() ) {
			this->m_pendingUpdates[valueId_] = std::make_shared<PendingUpdate>( source_ );
		}
		
		// A reference to the pending update is made here. Note that when the thread is entered, a copy is made
		// of the variable, so it can be released at the end of the scope!
		std::shared_ptr<PendingUpdate>& pendingUpdate = this->m_pendingUpdates[valueId_];
		pendingUpdatesLock.unlock();
		
		if ( pendingUpdate->updateMutex.try_lock_for( std::chrono::milliseconds( OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC ) ) ) {
			pendingUpdate->source = source_;
			pendingUpdate->done = false;
			std::thread( [this,pendingUpdate,valueId_] { // pendingUpdate is copied into the thread (no &-sign)

				std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
				pendingUpdate->condition.wait_for( notifyLock, std::chrono::seconds( 15 ), [&pendingUpdate]{ return pendingUpdate->done; } );
				// Spurious wakeups are someting we have to live with; there's no way to determine if the amount
				// of time was passed or if a spurious wakeup occured.
				// TODO can the DONE parameter be by gone bye bye?

				std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
				auto search = this->m_pendingUpdates.find( valueId_ );
				if ( search != this->m_pendingUpdates.end() ) {
					this->m_pendingUpdates.erase( search );
				}
				pendingUpdatesLock.unlock();
				
				pendingUpdate->updateMutex.unlock();
				
				// TODO remove pending update
				
			} ).detach();

			return true;
		} else {
			return false;
		}
	};
	
}; // namespace micasa

#endif // WITH_OPENZWAVE
