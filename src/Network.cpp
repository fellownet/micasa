#include <future>
#include <regex>

#include "Network.h"
#include "Logger.h"

void micasa_mg_handler( mg_connection* mg_conn_, int event_, void* data_ ) {
	if ( mg_conn_->user_data ) {
		micasa::Network::Connection* connection = static_cast<micasa::Network::Connection*>( mg_conn_->user_data );
		connection->_handler( mg_conn_, event_, data_ );
	}
}

namespace micasa {

	using namespace nlohmann;
	
	// ==========
	// Connection
	// ==========

	Network::Connection::Connection( mg_connection* mg_conn_, t_eventFunc&& func_ ) :
		m_mg_conn( mg_conn_ ),
		m_func( std::move( func_ ) ),
		m_data( NULL ),
		m_flags( 0 )
	{
		this->m_openMutex.lock();
	};

	Network::Connection::Connection( mg_connection* mg_conn_, t_eventFunc& func_ ) :
		m_mg_conn( mg_conn_ ),
		m_func( std::ref( func_ ) ),
		m_data( NULL ),
		m_flags( 0 )
	{
		this->m_openMutex.lock();
	};

	Network::Connection::~Connection() {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_WAIT ) == NETWORK_CONNECTION_FLAG_WAIT && "A connection should be properly waited for before destruction." );
#endif // _DEBUG
	};

	void Network::Connection::close( bool wait_ ) {
		this->m_mg_conn->flags |= MG_F_SEND_AND_CLOSE;
		this->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
		Network::get()._interrupt();
		this->wait();
	};

	void Network::Connection::terminate() {
		this->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
		Network::get()._interrupt();
	};

	void Network::Connection::wait() {
#ifdef _DEBUG
		bool success = this->m_openMutex.try_lock_for( std::chrono::seconds( 5 ) );
		assert( success && "Waiting for a connection to close shouldn't take more than 5 seconds > is it called during an event?" );
		std::lock_guard<std::timed_mutex> openLock( this->m_openMutex, std::adopt_lock );
#else
		std::lock_guard<std::mutex> openLock( this->m_openMutex );
#endif // _DEBUG
		this->m_flags |= NETWORK_CONNECTION_FLAG_WAIT;
	};

	void Network::Connection::serve( const std::string& root_, const std::string& index_ ) {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		mg_serve_http_opts options;
		memset( &options, 0, sizeof( options ) );
		options.document_root = root_.c_str();
		options.index_files = index_.c_str();
		options.enable_directory_listing = "no";
		mg_serve_http( this->m_mg_conn, (http_message*)this->m_data, options );
	};

	void Network::Connection::reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		this->m_tasks.push( std::bind( [data_,code_,headers_]( mg_connection* mg_conn_ ) {
			std::stringstream headers;
			for ( auto headersIt = headers_.begin(); headersIt != headers_.end(); ) {
				headers << headersIt->first << ": " << headersIt->second;
				if ( ++headersIt != headers_.end() ) {
					headers << "\r\n";
				}
			}
			mg_send_head( mg_conn_, code_, data_.length(), headers.str().c_str() );
			if ( code_ != 304 ) { // not modified
				mg_send( mg_conn_, data_.c_str(), data_.length() );
			}
			mg_conn_->flags |= MG_F_SEND_AND_CLOSE;
		}, this->m_mg_conn ) );
		this->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
		tasksLock.unlock();
		Network::get()._interrupt();
	};

	void Network::Connection::send( const std::string& data_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		this->m_tasks.push( std::bind( [data_]( mg_connection* mg_conn_ ) {
			mg_send( mg_conn_, data_.c_str(), data_.length() );
		}, this->m_mg_conn ) );
		tasksLock.unlock();
		Network::get()._interrupt();
	};

	std::string Network::Connection::getData() const {
		std::string data( this->m_mg_conn->recv_mbuf.buf, this->m_mg_conn->recv_mbuf.len );
		mbuf_remove( &this->m_mg_conn->recv_mbuf, this->m_mg_conn->recv_mbuf.len );
		return data;		
	};
	
	std::string Network::Connection::getResponse() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		http_message* http = (http_message*)this->m_data;
		return std::string( http->body.p, http->body.len );
	};

	std::string Network::Connection::getRequest() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		http_message* http = (http_message*)this->m_data;
		return std::string( http->body.p, http->body.len );
	};

	std::string Network::Connection::getUri() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		http_message* http = (http_message*)this->m_data;
		return std::string( http->uri.p, http->uri.len );
	};

	std::string Network::Connection::getQuery() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		http_message* http = (http_message*)this->m_data;
		return std::string( http->query_string.p, http->query_string.len );
	};
	
	std::string Network::Connection::getMethod() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		http_message* http = (http_message*)this->m_data;
		return std::string( http->method.p, http->method.len );
	};

	std::map<std::string, std::string> Network::Connection::getParams() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
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

	std::map<std::string, std::string> Network::Connection::getHeaders() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP && "A valid http_message should be present." );
#endif // _DEBUG
		http_message* http = (http_message*)this->m_data;
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

	void Network::Connection::_handler( mg_connection* mg_conn_, int event_, void* data_ ) {
		Network& network = Network::get();
		this->m_data = data_;

		switch( event_ ) {
			case MG_EV_ACCEPT: {
				std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn_, this->m_func );
				mg_conn_->user_data = connection.get();
				connection->m_flags |= NETWORK_CONNECTION_FLAG_WAIT;
				network.m_connections[mg_conn_] = connection;
				break;
			}

			case MG_EV_POLL: {
				// The broadcast method will trigger this event for all open connections. Any pending sending of data
				// needs to be done within this event.
				std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
				while( ! this->m_tasks.empty() ) {
					auto task = this->m_tasks.front();
					this->m_tasks.pop();
					task();
				}
				break;
			}

			case MG_EV_CONNECT: {
				mg_set_timer( this->m_mg_conn, 0 );
				int status = *(int*)data_;
				Connection::Event event = Connection::Event::CONNECT;
				if ( status != 0 ) {
					this->m_flags |= NETWORK_CONNECTION_FLAG_FAILURE;
					event = Connection::Event::FAILURE;
				}
				this->m_func( this->shared_from_this(), event );
				break;
			}

			case MG_EV_TIMER: {
				// If the timer event is fired the connection timed out and a failure event should be fired instead.
				// NOTE the connect event resets the timer.
				this->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
				this->m_flags |= NETWORK_CONNECTION_FLAG_FAILURE;
				this->m_func( this->shared_from_this(), Connection::Event::FAILURE );
				break;
			}

			case MG_EV_RECV: {
				this->m_func( this->shared_from_this(), Connection::Event::DATA );
				break;
			}

			case MG_EV_HTTP_REQUEST: {
				// This event is fired after a successfull incoming request has been made on a bind connection. The
				// connection is *not* closed to allow keep-alive requests.
				this->m_flags |= NETWORK_CONNECTION_FLAG_HTTP;
				this->m_func( this->shared_from_this(), Connection::Event::HTTP_REQUEST );
				this->m_flags &= ~NETWORK_CONNECTION_FLAG_HTTP;
				break;
			}

			case MG_EV_HTTP_REPLY: {
				// This event is fired after a successfull outgoing http request has been made using connect. Each
				// outgoing connection is closed immediately after a valid response.
				this->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
				this->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
				this->m_flags |= NETWORK_CONNECTION_FLAG_HTTP;
				this->m_func( this->shared_from_this(), Connection::Event::HTTP_RESPONSE );
				this->m_flags &= ~NETWORK_CONNECTION_FLAG_HTTP;
				break;
			}

			case MG_EV_CLOSE: {
				Connection::Event event = Connection::Event::DROPPED;
				if ( ( this->m_flags & NETWORK_CONNECTION_FLAG_CLOSE ) == NETWORK_CONNECTION_FLAG_CLOSE ) {
					event = Connection::Event::CLOSE;
				}
				if ( ( this->m_flags & NETWORK_CONNECTION_FLAG_FAILURE ) != NETWORK_CONNECTION_FLAG_FAILURE ) {
					this->m_func( this->shared_from_this(), event );
				}

				this->m_openMutex.unlock();
				network.m_connections.erase( mg_conn_ );
				break;
			}
		}
	};

	// =======
	// Network
	// =======

	Network::Network() {
		mg_mgr_init( &this->m_manager, NULL );
		this->m_shutdown = false;
		this->m_worker = std::thread( [this]() -> void {
			do {
				mg_mgr_poll( &this->m_manager, 5000 );
				std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
				while( ! this->m_tasks.empty() ) {
					auto task = this->m_tasks.front();
					this->m_tasks.pop();
					task();
				}
			} while( ! this->m_shutdown );
		} );
	};

	Network::~Network() {
		// First we're detaching all event handlers. When the network singleton is destructed all connections should've
		// been properly closed already.
		for ( auto const &connectionIt : this->m_connections ) {
			connectionIt.second->m_mg_conn->user_data = NULL;
		}

		this->m_shutdown = true;
		Network::_interrupt();
		this->m_worker.join();
		
		mg_mgr_free( &this->m_manager );

#ifdef _DEBUG
		assert( this->m_connections.empty() && "All connections should've been closed upon destruction of the network instance." );
#endif // _DEBUG
	};

	std::shared_ptr<Network::Connection> Network::bind( const std::string& port_, Connection::t_eventFunc&& func_ ) {
		mg_bind_opts options;
		memset( &options, 0, sizeof( options ) );
		options.user_data = NULL;
		return Network::_bind( port_, options, std::move( func_ ) );
	};

#ifdef _WITH_OPENSSL	
	std::shared_ptr<Network::Connection> Network::bind( const std::string& port_, const std::string& cert_, const std::string& key_, Connection::t_eventFunc&& func_ ) {
		mg_bind_opts options;
		memset( &options, 0, sizeof( options ) );
		options.user_data = NULL;
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
		std::promise<std::shared_ptr<Connection> > promise;
		std::future<std::shared_ptr<Connection> > future = promise.get_future();
		Network& network = Network::get();

		std::unique_lock<std::mutex> tasksLock( network.m_tasksMutex );
		network.m_tasks.push( std::bind( [&]( Network::Connection::t_eventFunc& func_ ) {
			mg_connection* mg_conn = mg_bind_opt( &network.m_manager, port_.c_str(), micasa_mg_handler, options_ );
			if ( mg_conn ) {
				mg_set_protocol_http_websocket( mg_conn );
				Logger::logr( Logger::LogLevel::VERBOSE, &network, "Binding to port %s.", port_.c_str() );
				std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn, std::move( func_ ) );
				mg_conn->user_data = connection.get();
				network.m_connections[mg_conn] = connection;
				promise.set_value( connection );
			} else {
				promise.set_value( nullptr );
			}
		}, std::move( func_ ) ) );
		tasksLock.unlock();

		network._interrupt();
		return future.get();
	};
	
	std::shared_ptr<Network::Connection> Network::_connect( const std::string& uri_, const nlohmann::json& data_, Network::Connection::t_eventFunc&& func_ ) {
		std::promise<std::shared_ptr<Connection> > promise;
		std::future<std::shared_ptr<Connection> > future = promise.get_future();
		Network& network = Network::get();

		std::unique_lock<std::mutex> tasksLock( network.m_tasksMutex );
		network.m_tasks.push( std::bind( [&]( Network::Connection::t_eventFunc& func_ ) {
			mg_connect_opts options;
			memset( &options, 0, sizeof( options ) );
			options.user_data = NULL;
			mg_connection* mg_conn;
			if ( uri_.substr( 0, 4 ) == "http" ) {
				if ( data_.is_null() ) {
					mg_conn = mg_connect_http_opt( &network.m_manager, micasa_mg_handler, options, uri_.c_str(), NULL, NULL );
				} else {
					mg_conn = mg_connect_http_opt( &network.m_manager, micasa_mg_handler, options, uri_.c_str(), "Content-Type: application/json\r\n", data_.dump().c_str() );
				}
				if ( mg_conn ) {
					mg_set_protocol_http_websocket( mg_conn );
				}
			} else {
				mg_conn = mg_connect_opt( &network.m_manager, uri_.c_str(), micasa_mg_handler, options );
			}
			if ( mg_conn ) {
				Logger::logr( Logger::LogLevel::VERBOSE, &network, "Connecting to %s.", uri_.c_str() );
				mg_set_timer( mg_conn, mg_time() + NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC );
				std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn, std::move( func_ ) );
				mg_conn->user_data = connection.get();
				network.m_connections[mg_conn] = connection;
				promise.set_value( connection );
			} else {
				promise.set_value( nullptr );
			}
		}, std::move( func_ ) ) );
		tasksLock.unlock();

		network._interrupt();
		return future.get();
	};

	void Network::_interrupt() {
		// The broadcast method should be called from a thread *other* then the poller thread. To enforce this, a new
		// thread is spawned.
		std::thread( std::bind( []( mg_mgr* mgr_ ) {
			mg_broadcast( mgr_, micasa_mg_handler, (void*)"", 0 );
		}, &this->m_manager ) ).detach();
	};

}; // namespace micasa
