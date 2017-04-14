#pragma once

#include <mutex>

#include "../Hardware.h"

#include "Notification.h"

#define OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC             1250 // how long to wait for a busy manager
#define OPEN_ZWAVE_IN_EXCLUSION_MODE_DURATION_MINUTES 1

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

namespace micasa {

	class ZWave final : public Hardware {

		friend class ZWaveNode;
		friend void ::micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

	public:
		static const char* label;

		ZWave( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~ZWave() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	private:
		static std::timed_mutex g_managerMutex;
		static unsigned int g_managerWatchers;
	
		std::string m_port;
		unsigned int m_homeId;
		
		void _handleNotification( const OpenZWave::Notification* notification_ );

	}; // class ZWave

}; // namespace micasa
