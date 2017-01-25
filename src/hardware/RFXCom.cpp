#include "RFXCom.h"

#include <sstream>

#include "../Logger.h"
#include "../Controller.h"
#include "../WebServer.h"
#include "../User.h"
#include "../Serial.h"
#include "../Utils.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;

	RFXCom::RFXCom( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) {
		g_webServer->addResourceCallback( {
			"hardware-" + std::to_string( this->m_id ),
			"^api/hardware/" + std::to_string( this->m_id ) + "$",
			99,
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const nlohmann::json& input_, const WebServer::Method& method_, nlohmann::json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				auto settings = extractSettingsFromJson( input_ );
				try {
					this->m_settings->put( "port", settings.at( "port" ) );
				} catch( std::out_of_range exception_ ) { };
				if ( this->m_settings->isDirty() ) {
					this->m_settings->commit();
					this->m_needsRestart = true;
				}
			} )
		} );
	};

	RFXCom::~RFXCom() {
		g_webServer->removeResourceCallback( "hardware-" + std::to_string( this->m_id ) );
	};

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

	bool RFXCom::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		// NOTE for now only LIGHTING2 devices are eligible to be set by the end-user, enhancements are needed if
		// more types are added.
		auto device = std::static_pointer_cast<Switch>( device_ );
		auto parts = stringSplit( device_->getReference(), '|' );
		unsigned long prefix = std::stoul( parts[0] );

		tRBUF packet;
		packet.LIGHTING2.id1 = ( prefix >> 24 ) & 0xff;
		packet.LIGHTING2.id2 = ( prefix >> 16 ) & 0xff;
		packet.LIGHTING2.id3 = ( prefix >> 8 ) & 0xff;
		packet.LIGHTING2.id4 = prefix & 0xff;
		packet.LIGHTING2.unitcode = std::stoi( parts[1] );
		packet.LIGHTING2.cmnd = ( device->getValueOption() == Switch::Option::ON ? light2_sOn : light2_sOff );
		
		this->m_serial->write( (unsigned char*)&packet, sizeof( packet ) );
		return true;
	};

	bool RFXCom::_processPacket() {

		// NOTE The packet is a union of all the available structs. All these structs have two bytes that indicate
		// the length and type of the packet. Casting it to an array of bytes and then taking the first two array
		// values will give us just that.
		BYTE length = ((BYTE*)&this->m_packet)[0];
		BYTE type = ((BYTE*)&this->m_packet)[1];
		if ( length < 1 ) {
			g_logger->log( Logger::LogLevel::WARNING, this, "Invalid packet received." );
			return false;
		}

		auto packet = reinterpret_cast<const tRBUF*>( &this->m_packet );

		switch( type ) {
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

		g_logger->logr( Logger::LogLevel::WARNING, this, "Unsupported packet type 0x%02X with length %d bytes.", type, length );
		return true;
	};

	bool RFXCom::_handleTempHumPacket( const tRBUF* packet_ ) {

		// TODO handle battery level and signal strength > maybe these are properties of a device and can
		// be beneficial to all devices?

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

		this->_declareDevice<Level>( reference + "(T)", "Temperature", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::TEMPERATURE ) },
			{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::CELSIUS ) }
		} )->updateValue( Device::UpdateSource::HARDWARE, temperature );

		unsigned int humidity = (unsigned int)packet_->TEMP_HUM.humidity;
		if ( humidity > 100 ) {
			return false;
		}

		this->_declareDevice<Level>( reference + "(H)", "Humidity", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE, Level::resolveSubType( Level::SubType::HUMIDITY ) },
			{ DEVICE_SETTING_DEFAULT_UNITS, Level::resolveUnit( Level::Unit::PERCENT ) }
		} )->updateValue( Device::UpdateSource::HARDWARE, humidity );

		return true;
	};

	bool RFXCom::_handleLightning2Packet( const tRBUF* packet_ ) {
		unsigned long prefix = 0;
		prefix |= ( packet_->LIGHTING2.id1 << 24 );
		prefix |= ( packet_->LIGHTING2.id2 << 16 );
		prefix |= ( packet_->LIGHTING2.id3 << 8 );
		prefix |= packet_->LIGHTING2.id4;

		// The group on/off commands do not create devices, they merely turn on/off existing devices
		// within the group.
		if (
			packet_->LIGHTING2.cmnd == light2_sGroupOn
			|| packet_->LIGHTING2.cmnd == light2_sGroupOff
		) {
			Switch::Option value = ( packet_->LIGHTING2.cmnd == light2_sGroupOn ? Switch::Option::ON : Switch::Option::OFF );
			auto devices = this->getAllDevices( std::to_string( prefix ) );
			for ( auto devicesIt = devices.begin(); devicesIt != devices.end(); devicesIt++ ) {
				auto device = std::static_pointer_cast<Switch>( *devicesIt );
				device->updateValue( Device::UpdateSource::HARDWARE, value );
			}
		} else {
			Switch::Option value = ( packet_->LIGHTING2.cmnd == light2_sOn ? Switch::Option::ON : Switch::Option::OFF );
			this->_declareDevice<Switch>( std::to_string( prefix ) + "|" + std::to_string( (int)packet_->LIGHTING2.unitcode ), "Switch", {
				{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) },
				{ DEVICE_SETTING_DEFAULT_SUBTYPE, Switch::resolveSubType( Switch::SubType::GENERIC ) }
			} )->updateValue( Device::UpdateSource::HARDWARE, value );
		}

		return true;
	};

}; // namespace micasa
