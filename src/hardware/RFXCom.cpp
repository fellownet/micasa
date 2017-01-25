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
						this->_processPacket();
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
			case pTypeLighting1:
				//return (pLen == 0x07);
				break;
			case pTypeLighting2:
				//return (pLen == 0x0B);
				break;
			case pTypeLighting3:
				//return (pLen == 0x08);
				break;
			case pTypeLighting4:
				//return (pLen == 0x09);
				break;
			case pTypeLighting5:
				//return (pLen == 0x0A);
				break;
			case pTypeLighting6:
				//return (pLen == 0x0B);
				break;
			case pTypeChime:
				//return (pLen == 0x07);
				break;
			case pTypeFan:
				//return (pLen == 0x08);
				break;
			case pTypeCurtain:
				//return (pLen == 0x07);
				break;
			case pTypeBlinds:
				//return (pLen == 0x09);
				break;
			case pTypeRFY:
				//return (pLen == 0x0C);
				break;
			case pTypeHomeConfort:
				//return (pLen == 0x0C);
				break;
			case pTypeSecurity1:
				//return (pLen == 0x08);
				break;
			case pTypeSecurity2:
				//return (pLen == 0x1C);
				break;
			case pTypeCamera:
				//return (pLen == 0x06);
				break;
			case pTypeRemote:
				//return (pLen == 0x06);
				break;
			case pTypeThermostat1:
				//return (pLen == 0x09);
				break;
			case pTypeThermostat2:
				//return (pLen == 0x06);
				break;
			case pTypeThermostat3:
				//return (pLen == 0x08);
				break;
			case pTypeRadiator1:
				//return (pLen == 0x0C);
				break;
			case pTypeBBQ:
				//return (pLen == 0x0A);
				break;
			case pTypeTEMP_RAIN:
				//return (pLen == 0x0A);
				break;
			case pTypeTEMP:
				//return (pLen == 0x08);
				break;
			case pTypeHUM:
				//return (pLen == 0x08);
				break;

			case pTypeTEMP_HUM:
				return ( length == sizeof( packet->TEMP_HUM ) && this->_handleTempHumPacket( packet ) );
				break;

			case pTypeBARO:
				//return (pLen == 0x09);
				break;
			case pTypeTEMP_HUM_BARO:
				//return (pLen == 0x0D);
				break;
			case pTypeRAIN:
				//return (pLen == 0x0B);
				break;
			case pTypeWIND:
				//return (pLen == 0x10);
				break;
			case pTypeUV:
				//return (pLen == 0x09);
				break;
			case pTypeDT:
				//return (pLen == 0x0D);
				break;
			case pTypeCURRENT:
				//return (pLen == 0x0D);
				break;
			case pTypeENERGY:
				//return (pLen == 0x11);
			case pTypeCURRENTENERGY:
				//return (pLen == 0x13);
				break;
			case pTypePOWER:
				//return (pLen == 0x0F);
				break;
			case pTypeWEIGHT:
				//return (pLen == 0x08);
				break;
			case pTypeCARTELECTRONIC:
				//return (pLen == 0x15);
				break;
			case pTypeRFXSensor:
				//return (pLen == 0x07);
				break;
			case pTypeRFXMeter:
				//return (pLen == 0x0A);
				break;
			case pTypeFS20:
				//return (pLen == 0x09);
				break;
		}
		
		g_logger->logr( Logger::LogLevel::WARNING, this, "Unsupported packet type %02X with length %d.", type, length );
		return false;
	};

	bool RFXCom::_handleTempHumPacket( const tRBUF* packet_ ) {
	
		std::cout << "TEMP HUM PACKET RECEIVED\n";
	
		return true;
/*
		std::string label;
		switch( subtype ) {
			case sTypeTH1:
				label = "TH1 - THGN122/123/132,THGR122/228/238/268";
				break;
			case sTypeTH2:
				label = "TH2 - THGR810,THGN800";
				break;
			case sTypeTH3:
				label = "TH3 - RTGR328";
				break;
			case sTypeTH4:
				label = "TH4 - THGR328";
				break;
			case sTypeTH5:
				label = "TH5 - WTGR800";
				break;
			case sTypeTH6:
				label = "TH6 - THGR918/928,THGRN228,THGN500";
				break;
			case sTypeTH7:
				label = "TH7 - Cresta, TFA TS34C";
				break;
			case sTypeTH8:
				label = "TH8 - WT260,WT260H,WT440H,WT450,WT450H";
				break;
			case sTypeTH9:
				label = "TH9 - Viking 02038, 02035 (02035 has no humidity), TSS320";
				break;
			case sTypeTH10:
				label = "TH10 - Rubicson/IW008T/TX95";
				break;
			case sTypeTH11:
				label = "TH11 - Oregon EW109";
				break;
			case sTypeTH12:
				label = "TH12 - Imagintronix/Opus TX300";
				break;
			case sTypeTH13:
				label = "TH13 - Alecto WS1700 and compatibles";
				break;
			case sTypeTH14:
				label = "TH14 - Alecto";
				break;
			default: {
				std::stringstream ss;
				ss << std::hex << "Unknown type " << (unsigned int)subtype << " for packet type " << (unsigned int)type;
				label = ss.str();
				break;
			}
		}
*/
	
	};

}; // namespace micasa
