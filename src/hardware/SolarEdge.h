#pragma once

#include <forward_list>

#include "../Hardware.h"

extern "C" {
	
#include "mongoose.h"
	
	static void mongoose_handler( mg_connection* connection_, int event_, void* instance_ );
	
} // extern "C"

namespace micasa {

	class SolarEdge final : public Hardware, public Worker {

		friend void ::mongoose_handler( struct mg_connection *connection_, int event_, void* instance_ );
		
	public:
		SolarEdge( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~SolarEdge() { };
		
		std::string toString() const;
		void start() override;
		void stop() override;
		std::chrono::milliseconds _work( unsigned long int iteration_ );
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	private:
		mg_mgr m_manager;
		int m_busy = 0;
		std::forward_list<std::string> m_inverters;
		std::mutex m_invertersMutex;

		void _processHttpReply( mg_connection* connection_, http_message* message_ );
		
	}; // class SolarEdge

}; // namespace micasa
