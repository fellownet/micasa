#include "P1Meter.h"

#include "../Logger.h"
#include "../Serial.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;
	
	void P1Meter::start() {
		if ( ! this->m_settings->contains( { "port", "baudrate" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		
		this->m_serial = std::make_shared<Serial>(
			this->m_settings->get( "port" ),
			this->m_settings->get<unsigned int>( "baudrate" ),
			Serial::CharacterSize::CHAR_SIZE_8,
			Serial::Parity::PARITY_NONE,
			Serial::StopBits::STOP_BITS_1,
			Serial::FlowControl::FLOW_CONTROL_NONE,
			std::make_shared<Serial::t_callback>( [this]( const unsigned char* data_, const size_t length_ ) {

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
	
	void P1Meter::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		try {
			this->m_serial->close();
		} catch( Serial::SerialException exception_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
		}
		this->m_serial = nullptr;

		Hardware::stop();
	};

	json P1Meter::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["port"] = this->m_settings->get( "port", "" );
		result["baudrate"] = this->m_settings->get<unsigned int>( "baudrate", 115200 );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json P1Meter::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		json setting = {
			{ "name", "port" },
			{ "label", "Port" },
			{ "type", "string" },
			{ "sort", 98 }
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
		result += {
			{ "name", "baudrate" },
			{ "label", "Baudrate" },
			{ "type", "list" },
			{ "options", {
				{
					{ "value", 9600 },
					{ "label", "9600" }
				}, {
					{ "value", 115200 },
					{ "label", "115200" }
				}
			} },
			{ "sort", 99 }
		};

		return result;
	};

}; // namespace micasa
