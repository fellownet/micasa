#pragma once

#ifdef WITH_OPENZWAVE

#include <mutex>

#include "../Hardware.h"

#include "Notification.h"

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

namespace micasa {

	class OpenZWave final : public Hardware {
		
		friend void ::micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );
		
	public:
		OpenZWave( const unsigned int id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~OpenZWave() { };
		
		void start() override;
		void stop() override;
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return false; };

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		
	private:
		unsigned int m_homeId;
		unsigned char m_controllerNodeId;
		static std::mutex s_managerMutex;

		void _handleNotification( const ::OpenZWave::Notification* notification_ );

	}; // class OpenZWave

}; // namespace micasa

#endif // WITH_OPENZWAVE
