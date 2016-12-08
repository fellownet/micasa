#pragma once

#ifdef WITH_OPENZWAVE

#include <mutex>

#include "../Hardware.h"

#include "Notification.h"

#define OPEN_ZWAVE_MANAGER_BUSY_WAIT_MSEC	250 // how long to wait for a busy manager
#define OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC		3000 // how long to block node while waiting for result

void micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );

namespace micasa {

	class OpenZWave final : public Hardware {
		
		friend class OpenZWaveNode;
		friend void ::micasa_openzwave_notification_handler( const ::OpenZWave::Notification* notification_, void* handler_ );
		
	public:
		enum State {
			STARTING = 1,
			IDLE,
			HEALING,
			INCLUSION_MODE,
			EXCLUSION_MODE
		}; // enum State
		
		OpenZWave( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Hardware( id_, reference_, parent_, label_ ) { };
		~OpenZWave() { };
		
		void start() override;
		void stop() override;
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) { return false; };

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		
	private:
		mutable std::timed_mutex m_managerMutex;
		unsigned int m_homeId;
		unsigned char m_controllerNodeId;
		State m_controllerState = STARTING;

		void _handleNotification( const ::OpenZWave::Notification* notification_ );

	}; // class OpenZWave

}; // namespace micasa

#endif // WITH_OPENZWAVE
