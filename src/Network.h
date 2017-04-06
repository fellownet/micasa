#pragma once

#include <mutex>
#include <thread>
#include <map>
#include <queue>
#include <atomic>

#include "Utils.h"
#include "Scheduler.h"

#include "json.hpp"

extern "C" {
	#include "mongoose.h"

	void micasa_mg_handler( mg_connection* connection_, int event_, void* data_ );
} // extern "C"

#define NETWORK_CONNECTION_FLAG_CLOSE   (1 << 0) // close was called on the connection
#define NETWORK_CONNECTION_FLAG_FAILURE (1 << 1) // a failure event has been fired
#define NETWORK_CONNECTION_FLAG_HTTP    (1 << 2) // a valid http_message is available
#define NETWORK_CONNECTION_FLAG_WAIT    (1 << 3) // the connection has been waited for

#define NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC 10

namespace micasa {

	// =======
	// Network
	// =======

	class Network final {

	public:

		// ==========
		// Connection
		// ==========

		class Connection : public std::enable_shared_from_this<Connection> {

			friend class micasa::Network;
			friend void (::micasa_mg_handler)( mg_connection* connection_, int event_, void* data_ );

		public:
			enum class Event: unsigned short {
				CONNECT        = 1,
				DATA           = 2,
				HTTP_RESPONSE  = 50,
				HTTP_REQUEST   = 51,
				CLOSE          = 100,
				FAILURE        = 198,
				DROPPED        = 199
			}; // enum class Event
			ENUM_UTIL( Event );

			typedef std::function<void( std::shared_ptr<Connection> connection_, Event event_ )> t_eventFunc;

			Connection( mg_connection* connection_, t_eventFunc&& func_ );
			Connection( mg_connection* connection_, t_eventFunc& func_ );
			~Connection();

			Connection( const Connection& ) = delete; // do not copy
			Connection& operator=( const Connection& ) = delete; // do not copy-assign
			Connection( const Connection&& ) = delete; // do not move
			Connection& operator=( Connection&& ) = delete; // do not move-assign

			void close( bool wait_ = false );
			void terminate();
			void wait();
			void serve( const std::string& root_, const std::string& index_ = "index.html" );
			void reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_ );
			void send( const std::string& data_ );

			std::string getData() const;
			std::string getResponse() const;
			std::string getRequest() const;
			std::string getUri() const;
			std::string getQuery() const;
			std::string getMethod() const;
			std::map<std::string, std::string> getParams() const;
			std::map<std::string, std::string> getHeaders() const;

		private:
			mg_connection* m_mg_conn;
			t_eventFunc m_func;
			std::queue<std::function<void(void)> > m_tasks;
			mutable std::mutex m_tasksMutex;
			void* m_data;
			unsigned int m_flags;
#ifdef _DEBUG
			mutable std::timed_mutex m_openMutex;
#else
			mutable std::mutex m_openMutex;
#endif // _DEBUG

			void _handler( mg_connection* connection_, int event_, void* data_ );

		}; // class Connection

		Network( const Network& ) = delete; // do not copy
		Network& operator=( const Network& ) = delete; // do not copy-assign
		Network( const Network&& ) = delete; // do not move
		Network& operator=( Network&& ) = delete; // do not move-assign

		friend std::ostream& operator<<( std::ostream& out_, const Network* network_ ) { out_ << "Network"; return out_; }

		static std::shared_ptr<Connection> bind( const std::string& port_, Connection::t_eventFunc&& func_ );
#ifdef _WITH_OPENSSL
		static std::shared_ptr<Connection> bind( const std::string& port_, const std::string& cert_, const std::string& key_, Connection::t_eventFunc&& func_ );
#endif
		static std::shared_ptr<Connection> connect( const std::string& uri_, const nlohmann::json& data_, Connection::t_eventFunc&& func_ );
		static std::shared_ptr<Connection> connect( const std::string& uri_, Connection::t_eventFunc&& func_ );

	private:
		mg_mgr m_manager;
		std::atomic<bool> m_shutdown;
		std::thread m_worker;
		// NOTE the m_connections map is only accessed from within the poll thread, so no mutex is necessary.
		std::map<mg_connection*, std::shared_ptr<Connection> > m_connections;
		std::queue<std::function<void(void)> > m_tasks;
		mutable std::mutex m_tasksMutex;

		Network(); // private constructor
		~Network(); // private destructor

		static Network& get() {
			// In c++11 static initialization is supposed to be thread-safe.
			static Network instance;
			return instance;
		}

		static std::shared_ptr<Connection> _bind( const std::string& port_, const mg_bind_opts& options_, Connection::t_eventFunc&& func_ );
		static std::shared_ptr<Connection> _connect( const std::string& uri_, const nlohmann::json& data_, Connection::t_eventFunc&& func_ );

		void _interrupt();

	}; // class Network

}; // namespace micasa
