#pragma once

#include <mutex>

#include "../Hardware.h"

#include "Notification.h"

#define OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC 250 // how long to wait for a busy manager

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

namespace micasa {

	using namespace OpenZWave;

	class ZWave final : public Hardware {

		friend class ZWaveNode;
		friend void ::micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

	public:
		ZWave( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~ZWave();

		void start() override;
		void stop() override;

		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return false; };
		json getJson( bool full_ = false ) const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) throw() override { return std::chrono::milliseconds( 1000 * 60 * 5 ); }

	private:
		static std::timed_mutex g_managerMutex;
		static unsigned int g_managerWatchers;
	
		std::string m_port;
		unsigned int m_homeId = 0;
		
		void _handleNotification( const Notification* notification_ );
		void _installResourceHandlers() const;

	}; // class ZWave

}; // namespace micasa
