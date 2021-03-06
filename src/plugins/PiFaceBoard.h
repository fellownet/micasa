#pragma once

#include <atomic>

#include "../Plugin.h"

#include "../device/Level.h"
#include "../device/Counter.h"

#define PIFACEBOARD_BUSY_WAIT_MSEC         1000 // how long to wait for result
#define PIFACEBOARD_BUSY_BLOCK_MSEC        500  // how long to block activies while waiting for result
#define PIFACEBOARD_TOGGLE_WAIT_MSEC       1500 // how long between toggles
#define PIFACEBOARD_PROCESS_INTERVAL_MSEC  20
#define PIFACEBOARD_PORT_INPUT             0
#define PIFACEBOARD_PORT_OUTPUT            1

namespace micasa {

	class PiFaceBoard final : public Plugin {

	public:
		static const char* label;

		PiFaceBoard( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : Plugin( id_, type_, reference_, parent_ ) { };
		~PiFaceBoard() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		void updateDeviceJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void updateDeviceSettingsJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;

	private:
		std::atomic<bool> m_shutdown;
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
