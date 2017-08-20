#pragma once

#include <mutex>

#include "../Plugin.h"

#include "Notification.h"

#define OPEN_ZWAVE_MANAGER_TRY_LOCK_MSEC              100
#define OPEN_ZWAVE_MANAGER_TRY_LOCK_DURATION_MSEC     5000
#define OPEN_ZWAVE_MANAGER_TRY_LOCK_ATTEMPTS          10
#define OPEN_ZWAVE_IN_EXCLUSION_MODE_DURATION_MINUTES 1

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

namespace micasa {

	class ZWave final : public Plugin {

		friend class ZWaveNode;
		friend void ::micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

	public:
		static const char* label;

		ZWave( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~ZWave() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );

	private:
		static std::timed_mutex g_managerMutex;
		static unsigned int g_managerWatchers;

		std::string m_port;
		unsigned int m_homeId;

		void _handleNotification( const OpenZWave::Notification* notification_ );

	}; // class ZWave

}; // namespace micasa
