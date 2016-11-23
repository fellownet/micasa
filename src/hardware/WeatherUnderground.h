#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <chrono>

#include "../Hardware.h"
#include "../Worker.h"
#include "../WebServer.h"

extern "C" {
	
	#include "mongoose.h"
	
	void wu_mg_handler( mg_connection* connection_, int event_, void* instance_ );
	
} // extern "C"

namespace micasa {

	class WeatherUnderground final : public Hardware, public Worker {

		friend void ::wu_mg_handler( struct mg_connection *connection_, int event_, void* instance_ );
		
	public:
		WeatherUnderground( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ), Worker() { };
		~WeatherUnderground() { };
		
		std::string toString() const;
		void start() override;
		void stop() override;
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ );
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		
	private:
		bool m_busy = false;
	
		void _processHttpReply( mg_connection* connection_, const http_message* message_ );
	
	}; // class WeatherUnderground

}; // namespace micasa
