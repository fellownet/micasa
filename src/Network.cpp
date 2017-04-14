#include <regex>
#include <sstream>

#include "Network.h"
#include "Logger.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

void micasa_mg_handler( mg_connection* mg_conn_, int event_, void* data_ ) {
	micasa::Network::get()._handler( mg_conn_, event_, data_ );
}

namespace micasa {

	using namespace nlohmann;
	
	// ==========
	// Connection
	// ==========

	Network::Connection::Connection( mg_connection* mg_conn_, const std::string& uri_, unsigned int flags_, t_eventFunc&& func_ ) :
		m_mg_conn( mg_conn_ ),
		m_conn_uri( uri_ ),
		m_flags( flags_ ),
		m_func( std::move( func_ ) )
	{
	};

	Network::Connection::Connection( mg_connection* mg_conn_, const std::string& uri_, unsigned int flags_, const t_eventFunc& func_ ) :
		m_mg_conn( mg_conn_ ),
		m_conn_uri( uri_ ),
		m_flags( flags_ ),
		m_func( func_ )
	{
	};

	Network::Connection::~Connection() {
		if ( this->m_mg_conn != nullptr ) {
			this->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
		}
	};

	void Network::Connection::close() {
		if ( this->m_mg_conn != nullptr ) {
			this->m_mg_conn->flags |= MG_F_SEND_AND_CLOSE;
		}
		this->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
	};

	void Network::Connection::terminate() {
		this->m_func = nullptr;
		if ( this->m_mg_conn != nullptr ) {
			this->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
		}
	};

	void Network::Connection::serve( const std::string& root_, const std::string& index_ ) {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & MG_EV_HTTP_REQUEST ) == MG_EV_HTTP_REQUEST
			&& "Serving static content should be done from the REQUEST event."
		);
#endif // _DEBUG
		mg_serve_http_opts options;
		memset( &options, 0, sizeof( options ) );
		options.document_root = root_.c_str();
		options.index_files = index_.c_str();
		options.enable_directory_listing = "no";
		mg_serve_http( this->m_mg_conn, (http_message*)this->m_data, options );
	};

	void Network::Connection::reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_ ) {
		auto task = [this,data_,code_,headers_]() {
			std::stringstream headers;
			for ( auto headersIt = headers_.begin(); headersIt != headers_.end(); ) {
				headers << headersIt->first << ": " << headersIt->second;
				if ( ++headersIt != headers_.end() ) {
					headers << "\r\n";
				}
			}
			if ( this->m_mg_conn != nullptr ) {
				mg_send_head( this->m_mg_conn, code_, data_.length(), headers.str().c_str() );
				if ( code_ != 304 ) { // not modified
					mg_send( this->m_mg_conn, data_.c_str(), data_.length() );
				}
				this->m_mg_conn->flags |= MG_F_SEND_AND_CLOSE;
			}
		};
		if ( __unlikely( std::this_thread::get_id() == Network::get().m_worker.get_id() ) ) {
			task();
		} else {
			std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
			this->m_tasks.push( task );
			tasksLock.unlock();
			mg_broadcast( &Network::get().m_manager, micasa_mg_handler, (void*)"", 0 );
		}
	};

	void Network::Connection::send( const std::string& data_ ) {
		auto task = [this,data_]() {
			if ( this->m_mg_conn != nullptr ) {
				mg_send( this->m_mg_conn, data_.c_str(), data_.length() );
			}
		};
		if ( __likely( std::this_thread::get_id() == Network::get().m_worker.get_id() ) ) {
			task();
		} else {
			std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
			this->m_tasks.push( task );
			tasksLock.unlock();
			mg_broadcast( &Network::get().m_manager, micasa_mg_handler, (void*)"", 0 );
		}
	};

	std::string Network::Connection::popData() {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & MG_EV_RECV ) == MG_EV_RECV
			&& "Popping data from the connection should be done from the DATA callback."
		);
#endif // _DEBUG
		std::string data( this->m_mg_conn->recv_mbuf.buf, this->m_mg_conn->recv_mbuf.len );
		mbuf_remove( &this->m_mg_conn->recv_mbuf, this->m_mg_conn->recv_mbuf.len );
		return data;
	};

	std::string Network::Connection::getBody() const {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & ( MG_EV_HTTP_REQUEST | MG_EV_HTTP_REPLY ) ) > 0
			&& "Getting data from a connection should be done from within a callback."
		);
#endif // _DEBUG
		auto http = (http_message*)this->m_data;
		return std::string( http->body.p, http->body.len );
	};

	std::string Network::Connection::getUri() const {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & ( MG_EV_HTTP_REQUEST | MG_EV_HTTP_REPLY ) ) > 0
			&& "Getting data from a connection should be done from within a callback."
		);
#endif // _DEBUG
		auto http = (http_message*)this->m_data;
		return std::string( http->uri.p, http->uri.len );
	};

	std::string Network::Connection::getQuery() const {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & ( MG_EV_HTTP_REQUEST | MG_EV_HTTP_REPLY ) ) > 0
			&& "Getting data from a connection should be done from within a callback."
		);
#endif // _DEBUG
		auto http = (http_message*)this->m_data;
		return std::string( http->query_string.p, http->query_string.len );
	};

	std::string Network::Connection::getMethod() const {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & ( MG_EV_HTTP_REQUEST | MG_EV_HTTP_REPLY ) ) > 0
			&& "Getting data from a connection should be done from within a callback."
		);
#endif // _DEBUG
		auto http = (http_message*)this->m_data;
		return std::string( http->method.p, http->method.len );
	};

	std::map<std::string, std::string> Network::Connection::getHeaders() const {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & ( MG_EV_HTTP_REQUEST | MG_EV_HTTP_REPLY ) ) > 0
			&& "Getting data from a connection should be done from within a callback."
		);
#endif // _DEBUG
		auto http = (http_message*)this->m_data;
		std::map<std::string, std::string> headers;
		int i = 0;
		for ( const mg_str& name : http->header_names ) {
			std::string header, value;
			header.assign( name.p, name.len );
			if ( ! header.empty() ) {
				value.assign( http->header_values[i].p, http->header_values[i].len );
				headers[header] = value;
			}
			i++;
		}
		return headers;
	};

	std::map<std::string, std::string> Network::Connection::getParams() const {
#ifdef _DEBUG
		assert(
			std::this_thread::get_id() == Network::get().m_worker.get_id()
			&& ( this->m_event & ( MG_EV_HTTP_REQUEST | MG_EV_HTTP_REPLY ) ) > 0
			&& "Getting data from a connection should be done from within a callback."
		);
#endif // _DEBUG
		std::string query = this->getQuery();
		std::map<std::string, std::string> params;
		std::regex pattern( "([\\w+%]+)=([^&]*)" );
		for ( auto paramsIt = std::sregex_iterator( query.begin(), query.end(), pattern ); paramsIt != std::sregex_iterator(); paramsIt++ ) {
			std::string key = (*paramsIt)[1].str();
		
			unsigned int size = 1024;
			int length;
			std::string value;
			do {
				char* buffer = new char[size];
				length = mg_url_decode( (*paramsIt)[2].str().c_str(), (*paramsIt)[2].str().size(), buffer, size, 1 );
				if ( length > -1 ) {
					value.assign( buffer, length );
				}
				delete[] buffer;
				size *= 2;
				if ( size > 65536 ) {
					break;
				}
			} while( length == -1 );
			
			params[key] = value;
		}
		return params;
	};

	// =======
	// Network
	// =======

	Network::Network() {
		mg_mgr_init( &this->m_manager, NULL );
		this->m_shutdown = false;
		this->m_worker = std::thread( [this]() -> void {
			do {
				mg_mgr_poll( &this->m_manager, 100 );
			} while( ! this->m_shutdown );
		} );
	};

	Network::~Network() {
		for ( auto const &connectionIt : this->m_connections ) {
			connectionIt.second->m_func = nullptr;
			connectionIt.second->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
		}
		this->m_connections.clear();

		this->m_shutdown = true;
		this->m_worker.join();
		
		mg_mgr_free( &this->m_manager );

#ifdef _DEBUG
		assert( this->m_connections.size() == 0 && "All connections should've been closed when the manager is freed." );
#endif // _DEBUG
	};

	std::shared_ptr<Network::Connection> Network::bind( const std::string& port_, Connection::t_eventFunc&& func_ ) {
		mg_bind_opts options;
		memset( &options, 0, sizeof( options ) );
		return Network::_bind( port_, options, std::move( func_ ) );
	};

#ifdef _WITH_OPENSSL	
	std::shared_ptr<Network::Connection> Network::bind( const std::string& port_, const std::string& cert_, const std::string& key_, Connection::t_eventFunc&& func_ ) {
		mg_bind_opts options;
		memset( &options, 0, sizeof( options ) );
		if (
			cert_.size() > 0
			&& key_.size() > 0
		) {
			options.ssl_cert = cert_.c_str();
			options.ssl_key = key_.c_str();
		}
		return Network::_bind( port_, options, std::move( func_ ) );
	};
#endif

	std::shared_ptr<Network::Connection> Network::connect( const std::string& uri_, const json& data_, Network::Connection::t_eventFunc&& func_ ) {
		return Network::_connect( uri_, data_, std::move( func_ ) );
	};

	std::shared_ptr<Network::Connection> Network::connect( const std::string& uri_, Network::Connection::t_eventFunc&& func_ ) {
		return Network::_connect( uri_, nullptr, std::move( func_ ) );
	};

	std::shared_ptr<Network::Connection> Network::_bind( const std::string& port_, const mg_bind_opts& options_, Network::Connection::t_eventFunc&& func_ ) {
		Network& network = Network::get();
		mg_connection* mg_conn = mg_bind_opt( &network.m_manager, port_.c_str(), micasa_mg_handler, options_ );
		if ( mg_conn ) {
			mg_set_protocol_http_websocket( mg_conn );
			Logger::logr( Logger::LogLevel::VERBOSE, &network, "Binding to port %s.", port_.c_str() );
			std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn, "::" + port_, NETWORK_CONNECTION_FLAG_HTTP | NETWORK_CONNECTION_FLAG_BIND, std::move( func_ ) );
			mg_conn->user_data = mg_conn; // see ACCEPT event handler
			network.m_connections[mg_conn] = connection;
			return connection;
		} else {
			return nullptr;
		}
	};
	
	std::shared_ptr<Network::Connection> Network::_connect( const std::string& uri_, const nlohmann::json& data_, Network::Connection::t_eventFunc&& func_ ) {
		Network& network = Network::get();
		mg_connect_opts options;
		memset( &options, 0, sizeof( options ) );
		mg_connection* mg_conn;
		unsigned int flags = 0;
		if ( __likely( uri_.substr( 0, 4 ) == "http" ) ) {
			if ( data_.is_null() ) {
				mg_conn = mg_connect_http_opt( &network.m_manager, micasa_mg_handler, options, uri_.c_str(), NULL, NULL );
			} else {
				mg_conn = mg_connect_http_opt( &network.m_manager, micasa_mg_handler, options, uri_.c_str(), "Content-Type: application/json\r\n", data_.dump().c_str() );
			}
			if ( mg_conn ) {
				mg_set_protocol_http_websocket( mg_conn );
				flags |= NETWORK_CONNECTION_FLAG_HTTP;
			}
		} else {
			mg_conn = mg_connect_opt( &network.m_manager, uri_.c_str(), micasa_mg_handler, options );
		}
		if ( mg_conn ) {
			Logger::logr( Logger::LogLevel::VERBOSE, &network, "Connecting to %s.", uri_.c_str() );
			mg_set_timer( mg_conn, mg_time() + NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC );
			std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn, uri_, flags, std::move( func_ ) );
			network.m_connections[mg_conn] = connection;
			return connection;
		} else {
			return nullptr;
		}
	};

	inline void Network::_handler( mg_connection* mg_conn_, int event_, void* data_ ) {
		Network& network = Network::get();

		// The ACCEPT event is fired when a new connection enters a listening connection. The connection pointer points
		// to the *new* connection. The originating connection is stored in the user_data pointer .
		if ( event_ == MG_EV_ACCEPT ) {
			char addr[64];
			mg_sock_addr_to_str( (const socket_address*)data_, addr, sizeof( addr ), MG_SOCK_STRINGIFY_IP );
			Logger::logr( Logger::LogLevel::VERBOSE, &network, "Accept connection from %s.", addr );

			std::shared_ptr<Connection> bind = network.m_connections.at( (mg_connection*)mg_conn_->user_data );
			std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn_, addr, bind->m_flags & ~NETWORK_CONNECTION_FLAG_BIND, bind->m_func );
			network.m_connections[mg_conn_] = connection;
		}

		auto find = network.m_connections.find( mg_conn_ );
		if ( find == network.m_connections.end() ) {
			return;
		}
		std::shared_ptr<Connection> connection = find->second;
		connection->m_event = event_;
		connection->m_data = data_;
		switch( event_ ) {

			// The broadcast method will trigger this event for all open connections. Any pending sending of data
			// needs to be done within this event.
			case MG_EV_POLL: {
				std::unique_lock<std::mutex> tasksLock( connection->m_tasksMutex );
				while( ! connection->m_tasks.empty() ) {
					auto task = connection->m_tasks.front();
					connection->m_tasks.pop();
					task();
				}
				tasksLock.unlock();
				break;
			}

			// An ACCEPT event is fired as CONNECT.
			case MG_EV_ACCEPT: {
				if ( connection->m_func ) {
					connection->m_func( connection, Connection::Event::CONNECT );
				}
				break;
			}

			// When a connection is succesfully made the CONNECT event is fired. The timeout timer that was set when
			// the connection was initiated, is stopped.
			case MG_EV_CONNECT: {
				mg_set_timer( mg_conn_, 0 );
				int status = *(int*)data_;
				Connection::Event event = Connection::Event::CONNECT;
				if ( status != 0 ) {
					connection->m_flags |= NETWORK_CONNECTION_FLAG_FAILURE;
					event = Connection::Event::FAILURE;
				}
				if ( connection->m_func ) {
					connection->m_func( connection, event );
				}
				break;
			}

			// If the timer event is fired the connection timed out and a failure event should be fired instead.
			case MG_EV_TIMER: {
				mg_conn_->flags |= MG_F_CLOSE_IMMEDIATELY;
				connection->m_flags |= NETWORK_CONNECTION_FLAG_FAILURE;
				if ( connection->m_func ) {
					connection->m_func( connection, Connection::Event::FAILURE );
				}
				break;
			}

			// The RECV event is fired for partial data that is received from the connection. We're not refiring this
			// event for HTTP connections, these should use the getBody method.
			case MG_EV_RECV: {
				if ( ( connection->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == 0 ) {
					if ( connection->m_func ) {
						connection->m_func( connection, Connection::Event::DATA );
					}
				}
				break;
			}

			// If the connection protocol was set to http, both incoming as outgoing connections fire HTTP events.
			// During, and *only* during this event there's a http_message instance available. The serving of static
			// files requires this, so the SERVE event is fired synchronious with the poller..
			case MG_EV_HTTP_REQUEST:
			case MG_EV_HTTP_REPLY: {
				if ( connection->m_func ) {
					connection->m_func( connection, Connection::Event::HTTP );
				}
				if ( event_ == MG_EV_HTTP_REPLY ) {
					mg_conn_->flags |= MG_F_CLOSE_IMMEDIATELY;
					connection->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
				}
				break;
			}

			case MG_EV_CLOSE: {
				if ( ( connection->m_flags & NETWORK_CONNECTION_FLAG_FAILURE ) == 0 ) {
					Connection::Event event = Connection::Event::DROPPED;
					if ( ( connection->m_flags & NETWORK_CONNECTION_FLAG_CLOSE ) == NETWORK_CONNECTION_FLAG_CLOSE ) {
						event = Connection::Event::CLOSE;
					}
					if ( connection->m_func ) {
						connection->m_func( connection, event );
					}
				}
				network.m_connections.erase( mg_conn_ );
				connection->m_mg_conn = nullptr;
			}
		}
	};

}; // namespace micasa
