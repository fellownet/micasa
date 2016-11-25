#pragma once

#include "../Hardware.h"

namespace micasa {

	class HarmonyHub final : public Hardware, public Worker {
		
	public:
		enum ConnectionState {
			CLOSED = 1,
			CONNECTING,
			WAIT_FOR_CURRENT_ACTIVITY,
			WAIT_FOR_ACTIVITIES,
			IDLE,
		};
		
		HarmonyHub( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~HarmonyHub() { };
		
		void start() override;
		void stop() override;
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ );

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		
	private:
		mg_connection* m_connection;
		volatile ConnectionState m_state = CLOSED;
		std::string m_currentActivityId = "-1";
		// TODO use notify with std::condition_variable in addition to the lock below because the lock will be
		// held and unlocked from different threads.
		mutable std::timed_mutex m_commandMutex;
		volatile bool m_commandBusy = false;
	
		void _disconnect( const std::string message_ );
		void _processConnection( const bool ready_ );
		
	}; // class HarmonyHub

}; // namespace micasa
