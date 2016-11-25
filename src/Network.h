#pragma once

#include <thread>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "Logger.h"
#include "Worker.h"

extern "C" {
	
	#include "mongoose.h"
	
	void micasa_mg_handler( mg_connection* connection_, int event_, void* data_ );
	
} // extern "C"

#define MG_EV_SHUTDOWN 999

/*
 terminology:
	handler = internal
	callback = external
 */

namespace micasa {
	
	class Network final : public Worker {
		
		friend void ::micasa_mg_handler( struct mg_connection *connection_, int event_, void* data_ );
		
	public:
		enum Event {
			SUCCESS = 1,
			FAILURE,
			DATA,
			TIMER,
			CLOSE
		}; // enum Event

		Network();
		~Network();
		friend std::ostream& operator<<( std::ostream& out_, const Network* ) { out_ << "Network"; return out_; }

		void start();
		void stop();
		
		typedef std::function<void( mg_connection* connection_, int event_, void* data_ )> t_callback;
		
		mg_connection* bind( const std::string port_, const t_callback callback_ );
		mg_connection* bind( const std::string port_, const std::string cert_, const std::string key_, const t_callback callback_ );
		mg_connection* connect( const std::string uri_, const t_callback callback_ );

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );
		
	private:
		typedef std::function<bool( mg_connection* connection_, int event_, void* data_ )> t_handler;
		
		mg_mgr m_manager;
		std::thread m_worker;
		std::set<mg_connection*> m_bindings;
		
		Network::t_handler* _newHandler( const Network::t_callback callback_, const bool binding_ );
		
	}; // class WebServer
	
}; // namespace micasa
