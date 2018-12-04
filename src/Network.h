#pragma once

#include <thread>
#include <map>
#include <unordered_map>
#include <atomic>
#include <queue>
#include <mutex>
#include <climits>

#include "Utils.h"
#include "Scheduler.h"

extern "C" {
	#include "mongoose.h"

	void micasa_mg_handler( mg_connection* connection_, int event_, void* data_ );
} // extern "C"

#define NETWORK_CONNECTION_FLAG_CLOSE      (1 << 0) // close was called on the connection
#define NETWORK_CONNECTION_FLAG_FAILURE    (1 << 1) // a failure event has been fired
#define NETWORK_CONNECTION_FLAG_HTTP       (1 << 2)
#define NETWORK_CONNECTION_FLAG_BIND       (1 << 3)
#define NETWORK_CONNECTION_FLAG_SOCKET     (1 << 4)

#define NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC 10

namespace micasa {

	// The HomeKit plugin requires direct access to the mongoose connection and is therefore marked as a friend.
	class HomeKit;

	// =======
	// Network
	// =======

	class Network final {

		friend class System;

		friend void (::micasa_mg_handler)( mg_connection* connection_, int event_, void* data_ );

	public:

		// ==========
		// Connection
		// ==========

		class Connection {

			friend class Network;
			friend class HomeKit;

		public:
			enum class Event: unsigned short {
				CONNECT = 1,
				DATA    = 2,
				HTTP    = 50,
				CLOSE   = 100,
				FAILURE = 198,
				DROPPED = 199
			}; // enum class Event
			ENUM_UTIL( Event );

			typedef std::function<void( std::shared_ptr<Connection> connection_, Event event_ )> t_eventFunc;

			Connection( mg_connection* connection_, unsigned int flags_, t_eventFunc&& func_ );
			Connection( mg_connection* connection_, unsigned int flags_, const t_eventFunc& func_ );
			~Connection();

			Connection( const Connection& ) = delete; // do not copy
			Connection& operator=( const Connection& ) = delete; // do not copy-assign
			Connection( const Connection&& ) = delete; // do not move
			Connection& operator=( Connection&& ) = delete; // do not move-assign

			void close();
			void terminate();
			void serve( const std::string& root_, const std::string& index_ = "index.html" );
			void reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_, bool close_ = false );
			void send( const std::string& data_ );

			std::string getData() const;
			std::string popData( unsigned int length_ = UINT_MAX );
			std::string getBody() const;
			std::string getUri() const;
			unsigned int getPort() const;
			std::string getIp() const;
			std::string getQuery() const;
			std::string getMethod() const;
			std::map<std::string, std::string> getHeaders() const;
			std::map<std::string, std::string> getParams() const;

		private:
			mg_connection* m_mg_conn;
			std::atomic<unsigned int> m_flags;
			struct http_message m_http;
			std::string m_data;
			t_eventFunc m_func;
			std::queue<std::function<void(void)>> m_tasks;
			mutable std::mutex m_mutex;

			// The calls to mg_broadcast should by synchronized; if not, one call might absorb the results from the
			// internal call to mg_poll of another (mg_poll and mg_broadcast communicate with eachother using a socket
			// pair where mg_poll acknowledges a call to mg_broadcast).
			static std::mutex s_broadcastMutex;

		}; // class Connection

		~Network(); // public destructor

		Network( const Network& ) = delete; // do not copy
		Network& operator=( const Network& ) = delete; // do not copy-assign
		Network( const Network&& ) = delete; // do not move
		Network& operator=( Network&& ) = delete; // do not move-assign

		friend std::ostream& operator<<( std::ostream& out_, const Network* network_ ) { out_ << "Network"; return out_; }

		static std::shared_ptr<Connection> bind( const std::string& port_, Connection::t_eventFunc&& func_ );
#ifdef _WITH_OPENSSL
		static std::shared_ptr<Connection> bind( const std::string& port_, const std::string& cert_, const std::string& key_, Connection::t_eventFunc&& func_ );
#endif
		static std::shared_ptr<Connection> connect( const std::string& uri_, const std::map<std::string, std::string>& headers_, const std::string& data_, Connection::t_eventFunc&& func_ );
		static std::shared_ptr<Connection> connect( const std::string& uri_, const std::map<std::string, std::string>& headers_, Connection::t_eventFunc&& func_ );

	private:
		Scheduler m_scheduler;
		mg_mgr m_manager;
		std::atomic<bool> m_shutdown;
		std::thread m_worker;
		std::unordered_map<mg_connection*, std::shared_ptr<Connection>> m_connections;

		Network(); // private constructor

		static Network& get() {
			// In c++11 static initialization is supposed to be thread-safe.
			static Network instance;
			return instance;
		}

		static std::shared_ptr<Connection> _bind( const std::string& port_, const mg_bind_opts& options_, Connection::t_eventFunc&& func_ );
		static std::shared_ptr<Connection> _connect( const std::string& uri_, const std::map<std::string, std::string>& headers_, const std::string& data_, Connection::t_eventFunc&& func_ );
		static void _handler( mg_connection* mg_conn_, int event_, void* data_ );

	}; // class Network

}; // namespace micasa
