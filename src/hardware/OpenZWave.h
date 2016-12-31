#pragma once

#ifdef WITH_OPENZWAVE

#include <mutex>

#include "../Hardware.h"

#include "Notification.h"

#define OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC 250 // how long to wait for a busy manager

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

namespace micasa {

	class OpenZWave final : public Hardware {
		
		friend class OpenZWaveNode;
		friend void ::micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );
		
	public:
		OpenZWave( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~OpenZWave() { };
		
		void start() override;
		void stop() override;
		
		const std::string getLabel() const;
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) { return false; };

	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		
	private:
		mutable std::timed_mutex m_managerMutex;
		unsigned int m_homeId;
		unsigned char m_controllerNodeId;

		void _handleNotification( const ::OpenZWave::Notification* notification_ );
		void _installResourceHandlers() const;

	}; // class OpenZWave

}; // namespace micasa

#endif // WITH_OPENZWAVE
