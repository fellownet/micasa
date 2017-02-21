#include "RFXCom.h"

#include <sstream>

#include "../Logger.h"
#include "../Database.h"
#include "../User.h"
#include "../Utils.h"
#include "../Serial.h"
#include "../device/Level.h"
#include "../device/Switch.h"

#define RFXCOM_BUSY_WAIT_MSEC  30000 // how long to wait for result
#define RFXCOM_BUSY_BLOCK_MSEC 3000  // how long to block hardware while waiting for result

// TODO implement many more recognized types.

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;

	void RFXCom::start() {
		if ( ! this->m_settings->contains( { "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		this->m_serial = std::make_shared<Serial>(
			this->m_settings->get( "port" ),
			38400, // baud rate of rfxcom devices
			Serial::CharacterSize::CHAR_SIZE_8,
			Serial::Parity::PARITY_NONE,
			Serial::StopBits::STOP_BITS_1,
			Serial::FlowControl::FLOW_CONTROL_NONE,
			std::make_shared<Serial::t_callback>( [this]( const unsigned char* data_, const size_t length_ ) {
				size_t pos = 0;
				while ( pos < length_ ) {

					this->m_packet[this->m_packetPosition] = data_[pos];

					// Ignore first character if it's 00.
					if (
						this->m_packetPosition == 0
						&& this->m_packet[0] == 0
					) {
						continue;
					}

					this->m_packetPosition++;

					if ( this->m_packetPosition >= sizeof( this->m_packet ) - 1 ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Packet out of sync." );
						this->m_packetPosition = 0;
					}

					// The first (accepted) byte should contain the length of the packet.
					if ( this->m_packetPosition > this->m_packet[0] ) {
						if ( false == this->_processPacket() ) {
							g_logger->logr( Logger::LogLevel::ERROR, this, "Error while processing packet." );
						}
						this->m_packetPosition = 0;
					}

					pos++;
				}
			} )
		);

		try {
			this->m_serial->open();
			this->setState( Hardware::State::READY );
		} catch( Serial::SerialException exception_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
			this->setState( Hardware::State::FAILED );
		}

		Hardware::start();

		// Add the devices that allow the user to create new devices. NOTE this has to be done *after* the
		// parent hardware instance is started to make sure previously created devices get picked up by the
		// declareDevice method.
		this->declareDevice<Switch>( "create_switch_device", "Add Switch Device", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
	};

	void RFXCom::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		try {
			this->m_serial->close();
		} catch( Serial::SerialException exception_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
		}
		this->m_serial = nullptr;

		Hardware::stop();
	};

	json RFXCom::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["port"] = this->m_settings->get( "port", "" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json RFXCom::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		json setting = {
			{ "name", "port" },
			{ "label", "Port" },
			{ "type", "string" },
			{ "class", this->m_settings->contains( "port" ) ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
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

		result += setting;
		return result;
	};

	json RFXCom::getDeviceJson( std::shared_ptr<const Device> device_, bool full_ ) const {
		json result = json::object();
		
		if ( device_->getSettings()->get<bool>( DEVICE_SETTING_ADDED_MANUALLY, false ) ) {
			result["rfx_type"] = device_->getSettings()->get( "rfx_type", "rfy" );
			if ( device_->getSettings()->get( "rfx_type", "" ) == "rfy" ) {
				result["rfx_id1"] = device_->getSettings()->get<unsigned int>( "rfx_id1", 0 );
				result["rfx_id2"] = device_->getSettings()->get<unsigned int>( "rfx_id2", 0 );
				result["rfx_id3"] = device_->getSettings()->get<unsigned int>( "rfx_id3", 0 );
				result["rfx_unitcode"] = device_->getSettings()->get<unsigned int>( "rfx_unitcode", 0 );
			}
		}
		
		return result;
	};

	json RFXCom::getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const {
		json result = json::array();

		if (
			device_->getSettings()->get<bool>( DEVICE_SETTING_ADDED_MANUALLY, false )
			&& device_->getType() == Device::Type::SWITCH
		) {
			json setting = {
				{ "name", "rfx_type" },
				{ "label", "Type" },
				{ "type", "list" },
				{ "class", device_->getSettings()->contains( "rfx_type" ) ? "advanced" : "normal" },
				{ "mandatory", true },
				{ "sort", 99 },
				{ "options", {
					{
						 { "value", "rfy" },
						 { "label", "RFY" },
						 { "settings", {
							{
								{ "name", "rfx_id1" },
								{ "label", "ID 1" },
								{ "type", "byte" },
								{ "minimum", 0 },
								{ "minimum", 255 },
								{ "mandatory", true }
							},
							{
								{ "name", "rfx_id2" },
								{ "label", "ID 2" },
								{ "type", "byte" },
								{ "minimum", 0 },
								{ "minimum", 255 },
								{ "mandatory", true }
							},
							{
								{ "name", "rfx_id3" },
								{ "label", "ID 3" },
								{ "type", "byte" },
								{ "minimum", 0 },
								{ "minimum", 255 },
								{ "mandatory", true }
							},
							{
								{ "name", "rfx_unitcode" },
								{ "label", "Unit Code" },
								{ "type", "byte" },
								{ "minimum", 1 },
								{ "minimum", 16 },
								{ "mandatory", true }
							}
						 } }
					},
					{
						 { "value", "x10" },
						 { "label", "X10" }
					},
					{
						 { "value", "arc" },
						 { "label", "ARC" }
					}
				} }
			};
			result += setting;
		}

		return result;
	};

	bool RFXCom::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		if ( this->getState() != Hardware::State::READY ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Hardware not ready." );
			return false;
		}

		// First actions on the built-in device that can be used to create custom rfxcom switches are handled.
		if (
			device_->getType() == Device::Type::SWITCH
			&& device_->getReference() == "create_switch_device"
		) {
			std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
			if ( device->getValueOption() == Switch::Option::ACTIVATE ) {
				this->declareDevice<Switch>( randomString( 16 ), "Switch", {
					{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
					{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveSubType( Switch::SubType::GENERIC ) },
					{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
					{ DEVICE_SETTING_ADDED_MANUALLY,         true }
				}, true )->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::OFF );
				this->wakeUpAfter( std::chrono::milliseconds( 1000 * 5 ) );
				return true;
			}
		}

		// Each device created by rfxcom (both manually and automatically) should have an rfx_type setting that
		// determines how to handle updates.
		std::string rfxType = device_->getSettings()->get( "rfx_type", "" );
		if (
			"rfy" == rfxType
			&& device_->getType() == Device::Type::SWITCH
		) {

			if ( this->_queuePendingUpdate( "rfxcom", source_, RFXCOM_BUSY_BLOCK_MSEC, RFXCOM_BUSY_WAIT_MSEC ) ) {
				auto device = std::static_pointer_cast<Switch>( device_ );

				tRBUF packet;
				packet.RFY.packetlength = sizeof( packet.RFY ) - 1;
				packet.RFY.packettype = pTypeRFY;
				packet.RFY.subtype = sTypeRFY;//sTypeRFYext;

				packet.RFY.id1 = device_->getSettings()->get<unsigned int>( "rfx_id1", 0 );
				packet.RFY.id2 = device_->getSettings()->get<unsigned int>( "rfx_id2", 0 );
				packet.RFY.id3 = device_->getSettings()->get<unsigned int>( "rfx_id3", 0 );
				packet.RFY.unitcode = device_->getSettings()->get<unsigned int>( "rfx_unitcode", 0 );

				if (
					device->getValueOption() == Switch::Option::ON
					|| device->getValueOption() == Switch::Option::OPEN
				) {
					packet.RFY.cmnd = rfy_sUp;
				} else if (
					device->getValueOption() == Switch::Option::OFF
					|| device->getValueOption() == Switch::Option::CLOSE
				) {
					packet.RFY.cmnd = rfy_sDown;
				} else if ( device->getValueOption() == Switch::Option::STOP ) {
					packet.RFY.cmnd = rfy_sStop;
				}

				this->m_serial->write( (unsigned char*)&packet, sizeof( packet.RFY ) );
				return true;
				
			} else {
				g_logger->log( Logger::LogLevel::ERROR, this, "Hardware busy." );
				return false;
			}
			
		} else if (
			"lighting2" == rfxType
			&& device_->getType() == Device::Type::SWITCH
		) {

			if ( this->_queuePendingUpdate( "rfxcom", source_, RFXCOM_BUSY_BLOCK_MSEC, RFXCOM_BUSY_WAIT_MSEC ) ) {
				auto device = std::static_pointer_cast<Switch>( device_ );
				auto parts = stringSplit( device->getReference(), '|' );
				unsigned long prefix = std::stoul( parts[0] );

				tRBUF packet;
				packet.LIGHTING2.packetlength = sizeof( packet.LIGHTING2 ) - 1;
				packet.LIGHTING2.packettype = pTypeLighting2;
				packet.LIGHTING2.subtype = std::stoi( parts[2] );

				packet.LIGHTING2.id1 = ( prefix >> 24 ) & 0xff;
				packet.LIGHTING2.id2 = ( prefix >> 16 ) & 0xff;
				packet.LIGHTING2.id3 = ( prefix >> 8 ) & 0xff;
				packet.LIGHTING2.id4 = prefix & 0xff;
				packet.LIGHTING2.unitcode = std::stoi( parts[1] );

				packet.LIGHTING2.cmnd = ( device->getValueOption() == Switch::Option::ON ? light2_sOn : light2_sOff );

				this->m_serial->write( (unsigned char*)&packet, sizeof( packet.LIGHTING2 ) );
				return true;

			} else {
				g_logger->log( Logger::LogLevel::ERROR, this, "Hardware busy." );
				return false;
			}
		}

		return false;
	};

	std::chrono::milliseconds RFXCom::_work( const unsigned long int& iteration_ ) {
		// Because the action devices needed to be created after the hardware was started, they might not exist in
		// the first iteration.
		if ( iteration_ > 1 ) {
			auto device = std::static_pointer_cast<Switch>( this->getDevice( "create_switch_device" ) );
			if ( device->getValueOption() == Switch::Option::ACTIVATE ) {
				device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
			}
		}
		return std::chrono::milliseconds( 1000 * 60 * 15 );
	};

	bool RFXCom::_processPacket() {

		// NOTE The packet is a union of all the available structs. All these structs have two bytes that indicate
		// the length and type of the packet. Casting it to an array of bytes and then taking the first two array
		// values will give us just that.
		BYTE length = ((BYTE*)&this->m_packet)[0];
		BYTE type = ((BYTE*)&this->m_packet)[1];
		BYTE subtype = ((BYTE*)&this->m_packet)[2];
		if ( length < 1 ) {
			g_logger->log( Logger::LogLevel::WARNING, this, "Invalid packet received." );
			return false;
		}

		auto packet = reinterpret_cast<const tRBUF*>( &this->m_packet );

		switch( type ) {
			case pTypeInterfaceMessage: {
				Device::UpdateSource source;
				this->_releasePendingUpdate( "rfxcom", source );
				// No return because this message needs to be logged.
				break;
			}
			
			case pTypeRecXmitMessage: {
				Device::UpdateSource source;
				this->_releasePendingUpdate( "rfxcom", source );
				return ( subtype == sTypeTransmitterResponse );
				break;
			}

			case pTypeTEMP_HUM:
				return ( length + 1 == sizeof( packet->TEMP_HUM ) && this->_handleTempHumPacket( packet ) );
				break;

			case pTypeLighting2:
				return ( length + 1 == sizeof( packet->LIGHTING2 ) && this->_handleLightning2Packet( packet ) );
				break;

			case pTypeLighting1:
			case pTypeLighting3:
			case pTypeLighting4:
			case pTypeLighting5:
			case pTypeLighting6:
			case pTypeChime:
			case pTypeFan:
			case pTypeCurtain:
			case pTypeBlinds:
			case pTypeRFY:
			case pTypeHomeConfort:
			case pTypeSecurity1:
			case pTypeSecurity2:
			case pTypeCamera:
			case pTypeRemote:
			case pTypeThermostat1:
			case pTypeThermostat2:
			case pTypeThermostat3:
			case pTypeRadiator1:
			case pTypeBBQ:
			case pTypeTEMP_RAIN:
			case pTypeTEMP:
			case pTypeHUM:
			case pTypeBARO:
			case pTypeTEMP_HUM_BARO:
			case pTypeRAIN:
			case pTypeWIND:
			case pTypeUV:
			case pTypeDT:
			case pTypeCURRENT:
			case pTypeENERGY:
			case pTypeCURRENTENERGY:
			case pTypePOWER:
			case pTypeWEIGHT:
			case pTypeCARTELECTRONIC:
			case pTypeRFXSensor:
			case pTypeRFXMeter:
			case pTypeFS20:
				break;
		}

		g_logger->logr( Logger::LogLevel::WARNING, this, "Unsupported packet type 0x%02X, subtype 0x%02X, length %d bytes.", type, subtype, length );
		return true;
	};

	bool RFXCom::_handleTempHumPacket( const tRBUF* packet_ ) {
		std::string reference = std::to_string( ( packet_->TEMP_HUM.id1 * 256 ) + packet_->TEMP_HUM.id2 );

		float temperature;
		if ( ! packet_->TEMP_HUM.tempsign ) {
			temperature = float( ( packet_->TEMP_HUM.temperatureh * 256 ) + packet_->TEMP_HUM.temperaturel ) / 10.0f;
		} else {
			temperature = -( float( ( ( packet_->TEMP_HUM.temperatureh & 0x7F ) * 256 ) + packet_->TEMP_HUM.temperaturel ) / 10.0f );
		}
		if (
			temperature < -200
			|| temperature > 380
		) {
			return false;
		}

		this->declareDevice<Level>( reference + "(T)", "Temperature", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::TEMPERATURE ) },
			{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::CELSIUS ) },
			{ DEVICE_SETTING_BATTERY_LEVEL,          (unsigned int)( packet_->TEMP_HUM.battery_level * 10 ) },
			{ DEVICE_SETTING_SIGNAL_STRENGTH,        (unsigned int)packet_->TEMP_HUM.rssi },
			{ "rfx_type", "temphum" }
		} )->updateValue( Device::UpdateSource::HARDWARE, temperature );

		unsigned int humidity = (unsigned int)packet_->TEMP_HUM.humidity;
		if ( humidity > 100 ) {
			return false;
		}

		this->declareDevice<Level>( reference + "(H)", "Humidity", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveSubType( Level::SubType::HUMIDITY ) },
			{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveUnit( Level::Unit::PERCENT ) },
			{ DEVICE_SETTING_BATTERY_LEVEL,          (unsigned int)( packet_->TEMP_HUM.battery_level * 10 ) },
			{ DEVICE_SETTING_SIGNAL_STRENGTH,        (unsigned int)packet_->TEMP_HUM.rssi },
			{ "rfx_type", "temphum" }
		} )->updateValue( Device::UpdateSource::HARDWARE, humidity );

		return true;
	};

	bool RFXCom::_handleLightning2Packet( const tRBUF* packet_ ) {

		unsigned long prefix = 0;
		prefix |= ( packet_->LIGHTING2.id1 << 24 );
		prefix |= ( packet_->LIGHTING2.id2 << 16 );
		prefix |= ( packet_->LIGHTING2.id3 << 8 );
		prefix |= packet_->LIGHTING2.id4;

		std::stringstream ss;
		ss << prefix << "|" << (int)packet_->LIGHTING2.unitcode << "|" << (int)packet_->LIGHTING2.subtype;
		std::string reference = ss.str();

		if (
			packet_->LIGHTING2.cmnd != light2_sGroupOn
			&& packet_->LIGHTING2.cmnd != light2_sGroupOff
		) {
			Switch::Option value = ( packet_->LIGHTING2.cmnd == light2_sOn ? Switch::Option::ON : Switch::Option::OFF );
			this->declareDevice<Switch>( reference, "Switch", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveSubType( Switch::SubType::GENERIC ) },
				{ DEVICE_SETTING_SIGNAL_STRENGTH,        (unsigned int)packet_->LIGHTING2.rssi },
				{ "rfx_type", "lighting2" }
			} )->updateValue( Device::UpdateSource::HARDWARE, value );
		}

		return true;
	};

}; // namespace micasa
