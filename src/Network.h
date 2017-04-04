#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <map>
#include <queue>
#include <atomic>

#include "Utils.h"

#include "json.hpp"

extern "C" {
	#include "mongoose.h"

	void micasa_mg_handler( mg_connection* connection_, int event_, void* data_ );
} // extern "C"

#define NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC 10

#define NETWORK_CONNECTION_FLAG_HTTP     (1 << 0) // this connection uses the http protocol handler
#define NETWORK_CONNECTION_FLAG_BIND     (1 << 1) // the connection is a listening connection
#define NETWORK_CONNECTION_FLAG_HAS_HTTP (1 << 2) // indicates that a valid http message is present
#define NETWORK_CONNECTION_FLAG_REQUEST  (1 << 3) // this connection is an incoming connection
#define NETWORK_CONNECTION_FLAG_JOINED   (1 << 4) // the connection has been joined after close

namespace micasa {

	// =======
	// Network
	// =======

	class Network final {

	public:

		// ==========
		// Connection
		// ==========

		class Connection {

			friend void (::micasa_mg_handler)( mg_connection* connection_, int event_, void* data_ );

		public:
			enum class Event: unsigned short {
				CONNECT = 1,
				FAILURE,
				DATA,
				DROPPED,
				CLOSE
			}; // enum class Event
			ENUM_UTIL( Event );

			typedef std::function<void( Connection& connection_, Event event_, const std::string& data_ )> t_eventFunc;

			Connection( mg_connection* connection_, t_eventFunc&& func_, unsigned short flags_ );
			~Connection();
			
			Connection( const Connection& ) = delete; // do not copy
			Connection& operator=( const Connection& ) = delete; // do not copy-assign
			Connection( const Connection&& ) = delete; // do not move
			Connection& operator=( Connection&& ) = delete; // do not move-assign

			void send( const std::string& data_ );
			void reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_ );
			void close();
			void terminate();
			void join();
			void serve( const std::string& root_, const std::string& index_ = "index.html" );
			std::string getUri() const { return this->m_uri; };
			std::string getQuery() const;
			std::string getMethod() const;
			std::map<std::string, std::string> getParams() const;
			std::map<std::string, std::string> getHeaders() const;

		private:
			std::string m_uri;
			mg_connection* m_connection;
			t_eventFunc m_func;
			unsigned short m_flags;
			http_message* m_http;
#ifdef _DEBUG
			mutable std::timed_mutex m_openMutex;
#else
			mutable std::mutex m_openMutex;
#endif // _DEBUG

			// Private constructor used to create a new Connection instance when a new incoming connection is accepted
			// by the handler.
			Connection( mg_connection* connection_, t_eventFunc& func_, unsigned short flags_ );

			void _handler( mg_connection* connection_, int event_, void* data_ );

		}; // class Connection

		friend class Connection;

		Network( const Network& ) = delete; // do not copy
		Network& operator=( const Network& ) = delete; // do not copy-assign

		static std::shared_ptr<Connection> bind( const std::string& port_, Connection::t_eventFunc&& func_ );
#ifdef _WITH_OPENSSL
		static std::shared_ptr<Connection> bind( const std::string& port_, const std::string& cert_, const std::string& key_, Connection::t_eventFunc&& func_ );
#endif
		static std::shared_ptr<Connection> connect( const std::string& uri_, Connection::t_eventFunc&& func_ = []( Connection&, Connection::Event, const std::string& ) { } );
		static std::shared_ptr<Connection> connect( const std::string& uri_, const std::string& data_, Connection::t_eventFunc&& func_ = []( Connection&, Connection::Event, const std::string& ) { } );

	private:
		mg_mgr m_manager;
		std::atomic<bool> m_shutdown;
		std::thread m_worker;
		std::queue<std::function<void(void)> > m_tasks;
		mutable std::recursive_mutex m_tasksMutex;

		Network(); // private constructor
		~Network(); // private destructor

		static Network& get() {
			// In c++11 static initialization is supposed to be thread-safe.
			static Network instance;
			return instance;
		}

		static mg_connection* _connect( const std::string& uri_ );
		static mg_connection* _connectHttp( const std::string& uri_ );
		static void _synchronize( std::function<void(void)>&& task_ );
		static void _wakeUp();

	}; // class Network

}; // namespace micasa
