#pragma once

#include <forward_list>

#include "../Hardware.h"

extern "C" {
	
	#include "mongoose.h"
	
	void solaredge_mg_handler( mg_connection* connection_, int event_, void* instance_ );
	
} // extern "C"

namespace micasa {

	class SolarEdge final : public Hardware, public Worker {

		friend void ::solaredge_mg_handler( struct mg_connection *connection_, int event_, void* instance_ );
		
	public:
		SolarEdge( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~SolarEdge() { };
		
		std::string toString() const;
		void start() override;
		void stop() override;
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

	private:
		int m_busy = 0;
		std::forward_list<std::string> m_inverters;
		std::mutex m_invertersMutex;

		void _processHttpReply( mg_connection* connection_, const http_message* message_ );
		
	}; // class SolarEdge

}; // namespace micasa
