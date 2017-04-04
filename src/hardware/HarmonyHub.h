#pragma once

#include "../Hardware.h"
#include "../Utils.h"
#include "../Network.h"

#define HARMONY_HUB_BUSY_WAIT_MSEC        60000 // how long to wait for result
#define HARMONY_HUB_BUSY_BLOCK_MSEC       3000  // how long to block activies while waiting for result
#define HARMONY_HUB_CONNECTION_ID         "21345678-1234-5678-1234-123456789012-1"
#define HARMONY_HUB_PING_INTERVAL_SEC     30
#define HARMONY_HUB_LAST_ACTIVITY_SETTING "_last_activity"

namespace micasa {

	class HarmonyHub final : public Hardware {
		
	public:
		enum class ConnectionState: unsigned short {
			WAIT_FOR_CURRENT_ACTIVITY,
			WAIT_FOR_ACTIVITIES,
			IDLE,
		}; // enum class ConnectionState
		ENUM_UTIL( ConnectionState );
		
		static const char* label;

		HarmonyHub( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~HarmonyHub() { };
		
		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return HarmonyHub::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		
	private:
		std::shared_ptr<Scheduler::Task<> > m_task;
		std::shared_ptr<Network::Connection> m_connection;
		ConnectionState m_connectionState = ConnectionState::IDLE;
		std::string m_currentActivityId = "-1";
		std::string m_received = "";
	
		bool _process();
		
	}; // class HarmonyHub

}; // namespace micasa
