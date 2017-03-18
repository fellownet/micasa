#pragma once

#include <thread>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "Utils.h"

extern "C" {
	#include "mongoose.h"
} // extern "C"

namespace micasa {
	
	class Network final {
		
	public:
		typedef std::function<bool( mg_connection* connection_, int event_, void* data_ )> t_handler;
		
		enum class Event: unsigned short {
			SUCCESS = 1,
			FAILURE,
			DATA,
			TIMER,
			CLOSE
		}; // enum class Event
		ENUM_UTIL( Event );

		typedef std::function<void( mg_connection* connection_, int event_, void* data_ )> t_callback;

		Network( const Network& ) = delete; // Do not copy!
		Network& operator=( const Network& ) = delete; // Do not copy-assign!

		friend std::ostream& operator<<( std::ostream& out_, const Network* ) { out_ << "Network"; return out_; }

		static Network& get() {
			// In c++11 static initialization is supposed to be thread-safe.
			static Network instance;
			return instance;
		}

		mg_connection* bind( const std::string port_, const t_callback callback_ );
		mg_connection* bind( const std::string port_, const std::string cert_, const std::string key_, const t_callback callback_ );
		mg_connection* connect( const std::string uri_, const t_callback callback_ );
		mg_connection* connect( const std::string uri_, const Network::t_callback callback_, const nlohmann::json& body_ );

	private:
		Network(); // private constructor
		~Network(); // private destructor

		mg_mgr m_manager;
		volatile bool m_shutdown = false;
		std::thread m_worker;
		std::set<mg_connection*> m_bindings;
		
		Network::t_handler* _newHandler( const Network::t_callback callback_, const bool binding_ );
		
	}; // class Network
	
}; // namespace micasa
