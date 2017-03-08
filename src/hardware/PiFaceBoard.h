#pragma once

#include "../Hardware.h"

#include "../device/Level.h"
#include "../device/Counter.h"

#define PIFACEBOARD_BUSY_WAIT_MSEC    1000 // how long to wait for result
#define PIFACEBOARD_BUSY_BLOCK_MSEC   500  // how long to block activies while waiting for result
#define PIFACEBOARD_WORK_WAIT_MSEC    50
#define PIFACEBOARD_TOGGLE_WAIT_MSEC  1500 // how long between toggles
#define PIFACEBOARD_MIN_COUNTER_PULSE 100 // the minimum duration of a counter pulse

#define PIFACEBOARD_PORT_INPUT      0
#define PIFACEBOARD_PORT_OUTPUT     1

namespace micasa {

	class PiFaceBoard final : public Hardware {

	public:
		static const constexpr char* label = "PiFace Board";

		struct CounterData {
			long initial;
			long session;
			unsigned long lastInterval;
			std::chrono::time_point<std::chrono::system_clock> lastPulse;
			std::chrono::time_point<std::chrono::system_clock> lastUpdate;
		}; // struct Counter

		PiFaceBoard( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~PiFaceBoard() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getDeviceJson( std::shared_ptr<const Device> device_, bool full_ = false ) const override;
		nlohmann::json getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

	private:
		std::shared_ptr<PiFace> m_parent;
		unsigned char m_devId;
		std::map<unsigned short, CounterData> m_counters;

		std::string _createReference( unsigned short position_, unsigned short io_ ) const;
		std::pair<unsigned short, unsigned short> _parseReference( const std::string& reference_ ) const;

	}; // class PiFaceBoard

}; // namespace micasa
