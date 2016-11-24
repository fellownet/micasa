#pragma once

#include "../Hardware.h"

extern "C" {
	
	#include "mongoose.h"
	
	void hhub_mg_handler( mg_connection* connection_, int event_, void* instance_ );
	
} // extern "C"

namespace micasa {

	class HarmonyHub final : public Hardware, public Worker {

		friend void ::hhub_mg_handler( struct mg_connection *connection_, int event_, void* instance_ );
		
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
		
		std::string toString() const;
		void start() override;
		void stop() override;
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ );

	private:
		mg_mgr m_manager;
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
