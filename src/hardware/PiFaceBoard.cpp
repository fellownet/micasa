#include "PiFace.h"
#include "PiFaceBoard.h"
#include "PiFace/MCP23x17.h"

#include "../Logger.h"
#include "../Utils.h"

#include "../device/Switch.h"

namespace micasa {

	using namespace std::chrono;
	using namespace nlohmann;
	
	const char* PiFaceBoard::label = "PiFace Board";

	void PiFaceBoard::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		// The parent has an open file descriptor to the SPI device which we need easy access to.
		this->m_parent = std::static_pointer_cast<PiFace>( this->getParent() );
		this->m_devId = std::stoi( this->m_reference );

		// Initialize the PiFace board.
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_IOCON, IOCON_INIT | IOCON_HAEN );
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_IOCONB, IOCON_INIT | IOCON_HAEN );

		// Setup Port A as output.
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_IODIRA, 0x00 ); // Set all pins on Port A as output

		// Lets init the other registers so we always have a clear startup situation.
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_GPINTENA, 0x00 ); // The PiFace does not support the interrupt capabilities, so set it to 0, and useless for output
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_DEFVALA, 0x00 ); // Default compare value register, useless for output
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_INTCONA, 0x00 ); // Interrupt based on current and prev pin state, not Default value
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_OLATA, 0xFF ); // Set the output latches to 1, otherwise the relays are activated

		// Setup Port B as input.
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_IODIRB, 0xFF ); // Set all pins on Port B as input
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_GPPUB, 0xFF ); // Enable pullup resistors, so the buttons work immediately
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_IPOLB, 0xFF ); // Invert input pin state, so we will see an active state as as 1

		// Lets init the other registers so we always have a clear startup situation.
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_GPINTENB, 0x00 ); // The PiFace does not support the interrupt capabilities, so set it to 0
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_DEFVALB, 0x00 ); // Default compare value register, we do not use it (yet), but lets set it to 0 (assuming not active state)
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_INTCONB, 0x00 ); // Interrupt based on current and prev pin state, not Default value
		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_OLATB, 0x00 ); // Set the output latches to 0, note: not used

		this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_GPIOA, 0x00 ); // Set all pins on Port A as output, and deactivate

		for ( unsigned short i = 0; i < 8; i++ ) {
			this->declareDevice<Switch>( this->_createReference( i, PIFACEBOARD_PORT_OUTPUT ), "Output " + std::to_string( i ), {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::GENERIC ) },
				{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true }
			} )->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::OFF );
			this->declareDevice<Switch>( this->_createReference( i, PIFACEBOARD_PORT_INPUT ), "Input " + std::to_string( i ), {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::HARDWARE ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::GENERIC ) },
				{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true }
			} )->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::OFF );

			this->m_inputs[i] = false;
			this->m_outputs[i] = false;
			this->m_lastPulse[i] = system_clock::now();
		}
		this->m_portState[0] = this->m_portState[1] = 0;

		this->setState( Hardware::State::READY );

		this->m_shutdown = false;
		this->m_worker = std::thread( [this]() -> void {
			unsigned long iteration = 0;
			while ( ! this->m_shutdown ) {
				this->_process( ++iteration );
				std::this_thread::sleep_for( std::chrono::milliseconds( PIFACEBOARD_PROCESS_INTERVAL_MSEC ) );
			}
		} );
	};
	
	void PiFaceBoard::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_shutdown = true;
		if ( this->m_worker.joinable() ) {
			this->m_worker.join();
		}
		Hardware::stop();
	};

	std::string PiFaceBoard::getLabel() const {
		std::stringstream label;
		label << PiFaceBoard::label << " " << this->m_reference;
		return label.str();
	};

	bool PiFaceBoard::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		auto device = std::static_pointer_cast<Switch>( device_ );
		auto port = this->_parseReference( device_->getReference() );
#ifdef _DEBUG
		assert( device_->getType() == Device::Type::SWITCH && "Device should be of Switch type." );
		assert( port.second == PIFACEBOARD_PORT_OUTPUT && "Device should be an output port." );
#endif // _DEBUG
			
		if ( this->_queuePendingUpdate( device->getReference(), source_, PIFACEBOARD_BUSY_BLOCK_MSEC, PIFACEBOARD_BUSY_WAIT_MSEC ) ) {
			unsigned char portState = this->m_parent->_Read_MCP23S17_Register( this->m_devId, MCP23x17_OLATA );
			int mask = 0x01;
			mask <<= port.first;
			if ( device->getValueOption() == Switch::Option::OFF ) {
				portState &= ~mask;
			} else {
				portState |= mask;
			}
			this->m_parent->_Write_MCP23S17_Register( this->m_devId, MCP23x17_GPIOA, portState );
			apply_ = false;
			return true;

		} else {
			Logger::log( Logger::LogLevel::ERROR, this, "PiFace Board busy." );
			return false;
		}
	};

	json PiFaceBoard::getDeviceJson( std::shared_ptr<const Device> device_, bool full_ ) const {
		json result = json::object();

		if ( device_->getType() == Device::Type::SWITCH ) {
			auto port = this->_parseReference( device_->getReference() );
			if ( port.second == PIFACEBOARD_PORT_INPUT ) {
				result["port_type"] = device_->getSettings()->get( "port_type", "pulse" );
				if ( device_->getSettings()->get( "port_type", "pulse" ) == "pulse" ) {
					result["count_pulses"] = device_->getSettings()->get<bool>( "count_pulses", false );
				}
			}
		}

		return result;
	};

	json PiFaceBoard::getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const {
		json result = json::array();

		if ( device_->getType() == Device::Type::SWITCH ) {
			auto port = this->_parseReference( device_->getReference() );
			if ( port.second == PIFACEBOARD_PORT_INPUT ) {
				json setting = {
					{ "name", "port_type" },
					{ "label", "Type" },
					{ "type", "list" },
					{ "mandatory", true },
					{ "sort", 99 },
					{ "options", {
						{
							{ "value", "pulse" },
							{ "label", "Pulse" },
							{ "settings", {
								{
									{ "name", "count_pulses" },
									{ "label", "Count Pulses" },
									{ "type", "boolean" }
								}
							} }
						},
						{
							{ "value", "toggle" },
							{ "label", "Toggle" }
						}
					} }
				};
				result += setting;
			}
		}

		return result;
	};

	void PiFaceBoard::_process( unsigned long iteration_ ) {

		// Read and process output pin states.
		unsigned char portState = this->m_parent->_Read_MCP23S17_Register( this->m_devId, MCP23x17_GPIOA );
		if ( this->m_portState[0] != portState ) {
			this->m_portState[0] = portState;

			int mask = 0x01;
			for ( unsigned short i = 0; i < 8; i++ ) {
				bool state = ( ( portState & mask ) == mask );
				if ( this->m_outputs[i] != state ) {
					this->m_outputs[i] = state;
					
					std::string reference = this->_createReference( i, PIFACEBOARD_PORT_OUTPUT );
					auto device = std::static_pointer_cast<Switch>( this->getDevice( reference ) );
					Switch::Option value = ( state ? Switch::Option::ON : Switch::Option::OFF );
					auto source = Device::UpdateSource::HARDWARE;
					this->_releasePendingUpdate( reference, source );
					device->updateValue( Device::UpdateSource::HARDWARE, value );
				}
				mask<<=1;
			}
		}

		// Read and process inputs pin states.
		portState = this->m_parent->_Read_MCP23S17_Register( this->m_devId, MCP23x17_GPIOB );
		if ( this->m_portState[1] != portState ) {
			this->m_portState[1] = portState;

			int mask = 0x01;
			for ( unsigned short i = 0; i < 8; i++ ) {
				bool state = ( ( portState & mask ) == mask );
				if ( this->m_inputs[i] != state ) {
					this->m_inputs[i] = state;

					std::string reference = this->_createReference( i, PIFACEBOARD_PORT_INPUT );
					auto device = std::static_pointer_cast<Switch>( this->getDevice( reference ) );
					Switch::Option value = ( state ? Switch::Option::ON : Switch::Option::OFF );
					std::string portType = device->getSettings()->get( "port_type", "pulse" );

					// Pulse ports can be set to "count", in which case separate counter/level devices are created that
					// report on how often a pulse is counted. If set to pulse a switch is created that remains in the
					// On position while the pulse is active (such as a doorbel).
					if ( portType == "pulse" ) {
						if ( ! device->getSettings()->get<bool>( "count_pulses", false ) ) {
							if ( device->getValueOption() != value ) {
								device->updateValue( Device::UpdateSource::HARDWARE, value );
							}
						} else {
							if ( value == Switch::Option::ON ) {
								if ( this->_queuePendingUpdate( device->getReference(), 0, PIFACEBOARD_MIN_COUNTER_PULSE_MSEC ) ) {
									this->declareDevice<Counter>( reference + "_counter", "Pulses " + std::to_string( i ), {
										{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::HARDWARE ) },
										{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::GENERIC ) },
										{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
										{ DEVICE_SETTING_ALLOW_UNIT_CHANGE,      true }
									} )->incrementValue( Device::UpdateSource::HARDWARE );
									if ( iteration_ >= 2 ) {
										unsigned long interval = duration_cast<milliseconds>( system_clock::now() - this->m_lastPulse[i] ).count();
										this->declareDevice<Level>( reference + "_level", "Pulses/sec " + std::to_string( i ), {
											{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::HARDWARE ) },
											{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::GENERIC ) },
											{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
											{ DEVICE_SETTING_ALLOW_UNIT_CHANGE,      true }
										} )->updateValue( Device::UpdateSource::HARDWARE, 1000.0f / interval );
									}
									this->m_lastPulse[i] = system_clock::now();
								} else {
									Logger::log( Logger::LogLevel::WARNING, this, "Ignoring pulse." );
								}
							}
						}

					// If the port is set to toggle a switch device is created. Each registered pulse on the port flips
					// the switch into the opposite position.
					} else if ( portType == "toggle" ) {
						if ( value == Switch::Option::ON ) {
							if ( this->_queuePendingUpdate( device->getReference(), 0, PIFACEBOARD_TOGGLE_WAIT_MSEC ) ) {
								auto old = device->getValueOption();
								if ( old == Switch::Option::ON ) {
									device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::OFF );
								} else {
									device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::ON );
								}
							} else {
								Logger::log( Logger::LogLevel::WARNING, this, "Ignoring toggle." );
							}
						}
					}
				}
				mask<<=1;
			}
		}
	};

	std::string PiFaceBoard::_createReference( unsigned short position_, unsigned short io_ ) const {
		std::stringstream reference;
		reference << this->m_reference << "_" << position_ << "_" << io_;
		return reference.str();
	};

	std::pair<unsigned short, unsigned short> PiFaceBoard::_parseReference( const std::string& reference_ ) const {
		auto parts = stringSplit( reference_, '_' );
		return { std::stoi( parts[1] ), std::stoi( parts[2] ) };
	};

}; // namespace micasa
