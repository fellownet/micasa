#pragma once

#include "../Hardware.h"

#include "../device/Level.h"
#include "../device/Counter.h"

#define PIFACEBOARD_BUSY_WAIT_MSEC         1000 // how long to wait for result
#define PIFACEBOARD_BUSY_BLOCK_MSEC        500  // how long to block activies while waiting for result
#define PIFACEBOARD_TOGGLE_WAIT_MSEC       1500 // how long between toggles
#define PIFACEBOARD_MIN_COUNTER_PULSE_MSEC 100 // the minimum duration of a counter pulse
#define PIFACEBOARD_PROCESS_INTERVAL_MSEC  20

#define PIFACEBOARD_PORT_INPUT      0
#define PIFACEBOARD_PORT_OUTPUT     1

namespace micasa {

	class PiFaceBoard final : public Hardware {

	public:
		static const constexpr char* label = "PiFace Board";

		PiFaceBoard( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~PiFaceBoard() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getDeviceJson( std::shared_ptr<const Device> device_, bool full_ = false ) const override;
		nlohmann::json getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const override;

	private:
		volatile bool m_shutdown = true;
		std::thread m_worker;
		std::shared_ptr<PiFace> m_parent;
		unsigned char m_devId;
		unsigned char m_portState[2];
		bool m_inputs[8];
		bool m_outputs[8];
		std::chrono::time_point<std::chrono::system_clock> m_lastPulse[8];

		void _process( unsigned long iteration_ );
		std::string _createReference( unsigned short position_, unsigned short io_ ) const;
		std::pair<unsigned short, unsigned short> _parseReference( const std::string& reference_ ) const;

	}; // class PiFaceBoard

}; // namespace micasa
