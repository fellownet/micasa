#include <regex>
#include <future>

#include "Network.h"

void micasa_mg_handler( mg_connection* connection_, int event_, void* data_ ) {
	if (
		MG_EV_POLL != event_
		&& connection_->user_data != NULL
	) {
		micasa::Network::Connection* connection = static_cast<micasa::Network::Connection*>( connection_->user_data );
		connection->_handler( connection_, event_, data_ );
	}
}

namespace micasa {
	
	// ==========
	// Connection
	// ==========

	Network::Connection::Connection( mg_connection* connection_, t_eventFunc&& func_, unsigned short flags_ ) :
		m_uri( "" ),
		m_connection( connection_ ),
		m_func( std::move( func_ ) ),
		m_flags( flags_ )
	{
		if ( this->m_connection != NULL ) {
			this->m_openMutex.lock();
			this->m_connection->user_data = this;
			if ( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP ) {
				mg_set_protocol_http_websocket( this->m_connection );
			}
			if ( (  this->m_flags & NETWORK_CONNECTION_FLAG_BIND ) == 0 ) {
				mg_set_timer( this->m_connection, mg_time() + NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC );
			}
		} else {
			this->m_func( *this, Event::FAILURE, "" );
		}
	}

	Network::Connection::Connection( mg_connection* connection_, t_eventFunc& func_, unsigned short flags_ ) :
		m_uri( "" ),
		m_connection( connection_ ),
		m_func( std::ref( func_ ) ),
		m_flags( flags_ )
	{
		if ( this->m_connection != NULL ) {
			this->m_openMutex.lock();
			this->m_connection->user_data = this;
			if ( (  this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == NETWORK_CONNECTION_FLAG_HTTP ) {
				mg_set_protocol_http_websocket( this->m_connection );
			}
		} else {
			this->m_func( *this, Event::FAILURE, "" );
		}
	}

	Network::Connection::~Connection() {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_JOINED ) == NETWORK_CONNECTION_FLAG_JOINED && "Connection should be joined before being destroyed." );
#endif // _DEBUG
	};

	void Network::Connection::send( const std::string& data_ ) {
		if ( this->m_connection != NULL ) {
			Network::_synchronize( [this,data_]() {
				mg_send( this->m_connection, data_.c_str(), data_.length() );
			} );
		}
	};

	void Network::Connection::reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_ ) {
		if ( this->m_connection != NULL ) {
			Network::_synchronize( [this,data_,code_,headers_]() {
				std::stringstream headers;
				for( auto headersIt = headers_.begin(); headersIt != headers_.end(); ) {
					headers << headersIt->first << ": " << headersIt->second;
					if ( ++headersIt != headers_.end() ) {
						headers << "\r\n";
					}
				}
				mg_send_head( this->m_connection, code_, data_.length(), headers.str().c_str() );
				if ( code_ != 304 ) { // not modified
					mg_send( this->m_connection, data_.c_str(), data_.length() );
				}
				this->m_connection->flags |= MG_F_SEND_AND_CLOSE;
				this->m_connection->flags |= MG_F_USER_1;
			} );
		}
	};

	void Network::Connection::close() {
		if ( this->m_connection != NULL ) {
			this->m_connection->flags |= MG_F_SEND_AND_CLOSE;
			this->m_connection->flags |= MG_F_USER_1;
			Network::_wakeUp();
		}
	};

	void Network::Connection::terminate() {
		if ( this->m_connection != NULL ) {
			this->m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
			Network::_wakeUp();
		}
	};

	void Network::Connection::join() {
#ifdef _DEBUG
		bool success = this->m_openMutex.try_lock_for( std::chrono::milliseconds( 5000 ) );
		assert( success && "Connections should not take more than 5 seconds to join." );
		std::lock_guard<std::timed_mutex> lock( this->m_openMutex, std::adopt_lock );
#else
		std::lock_guard<std::mutex> lock( this->m_openMutex );
#endif // _DEBUG
		this->m_flags |= NETWORK_CONNECTION_FLAG_JOINED;
	};
	
	void Network::Connection::serve( const std::string& root_, const std::string& index_ ) {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HAS_HTTP ) != 0 && "There should be a http message present before serving data." );
#endif // _DEBUG
		if ( this->m_connection != NULL ) {
			mg_serve_http_opts options;
			memset( &options, 0, sizeof( options ) );
			options.document_root = root_.c_str();
			options.index_files = index_.c_str();
			options.enable_directory_listing = "no";
			mg_serve_http( this->m_connection, this->m_http, options );
		}
	};

	std::string Network::Connection::getMethod() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HAS_HTTP ) != 0 && "There should be a http message present before accessing it's data." );
#endif // _DEBUG
		std::string method;
		method.assign( this->m_http->method.p, this->m_http->method.len );
		return method;
	};
	
	std::string Network::Connection::getQuery() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HAS_HTTP ) != 0 && "There should be a http message present before accessing it's data." );
#endif // _DEBUG
		std::string query;
		query.assign( this->m_http->query_string.p, this->m_http->query_string.len );
		return query;
	};

	std::map<std::string, std::string> Network::Connection::getParams() const {
#ifdef _DEBUG
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HAS_HTTP ) != 0 && "There should be a http message present before accessing it's data." );
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
		assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HAS_HTTP ) != 0 && "There should be a http message present before accessing it's data." );
#endif // _DEBUG
		std::map<std::string, std::string> headers;
		int i = 0;
		for ( const mg_str& name : this->m_http->header_names ) {
			std::string header, value;
			header.assign( name.p, name.len );
			if ( ! header.empty() ) {
				value.assign( this->m_http->header_values[i].p, this->m_http->header_values[i].len );
				headers[header] = value;
			}
			i++;
		}
		return headers;
	};

	void Network::Connection::_handler( mg_connection* connection_, int event_, void* data_ ) {
		switch( event_ ) {
			case MG_EV_ACCEPT: {
				// The accept event is called when a new connection is received on a bind connection. A new Connection
				// instance is created which we need to dispose of manually when the connection is closed. All future
				// events will be redirected to this new Connection instance.
				Connection* connectionIn = new Connection( connection_, this->m_func, NETWORK_CONNECTION_FLAG_REQUEST | NETWORK_CONNECTION_FLAG_HTTP );
				this->m_func( *connectionIn, Event::CONNECT, "" );
				break;
			}
			case MG_EV_CONNECT: {
				mg_set_timer( connection_, 0 );
				int status = *(int*)data_;
				if ( status == 0 ) {
					this->m_func( *this, Event::CONNECT, "" );
				} else {
					this->m_connection->flags |= MG_F_USER_2;
					this->m_func( *this, Event::FAILURE, "" );
				}
				break;
			}
			case MG_EV_TIMER: {
				this->m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
				this->m_connection->flags |= MG_F_USER_2;
				Network::_wakeUp();
				this->m_func( *this, Event::FAILURE, "" );
				break;
			}
			case MG_EV_RECV: {
				if ( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == 0 ) {
					std::string data( connection_->recv_mbuf.buf, connection_->recv_mbuf.len );
					mbuf_remove( &connection_->recv_mbuf, connection_->recv_mbuf.len );
					this->m_func( *this, Event::DATA, data );
				}
				break;
			}

			// NOTE only during a http event the data pointer ponts to a valid http_message. Once the event has finished
			// the pointer is invalid. Because we don't want to parse this message unless it's absolutely necessary, we
			// flag this connection when there's a valid http_message available. If the callback wants to handle the
			// connection asynchroniously it has to fetch all the required data from the http_message before spawning
			// a separate thread to handle this data.

			case MG_EV_HTTP_REQUEST: { // this is an INCOMING request
#ifdef _DEBUG
				assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) != 0 && "Connection should be of http type." );
#endif // _DEBUG
				http_message* message = (http_message*)data_;
				if ( message->uri.len >= 1 ) {
					this->m_uri.assign( message->uri.p + 1, message->uri.len - 1 );
				}
				this->m_http = message;
				this->m_flags |= NETWORK_CONNECTION_FLAG_HAS_HTTP;
				this->m_func( *this, Event::DATA, std::string( message->body.p, message->body.len ) );
				this->m_flags &= ~NETWORK_CONNECTION_FLAG_HAS_HTTP;
				break;
			}
			case MG_EV_HTTP_REPLY: { // this is an OUTGOING response
#ifdef _DEBUG
				assert( ( this->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) != 0 && "Connection should be of http type." );
#endif // _DEBUG
				http_message* message = (http_message*)data_;
				this->m_http = message;
				this->m_flags |= NETWORK_CONNECTION_FLAG_HAS_HTTP;
				this->m_func( *this, Event::DATA, std::string( message->body.p, message->body.len ) );
				this->close();
				this->m_flags &= ~NETWORK_CONNECTION_FLAG_HAS_HTTP;
				break;
			}

			case MG_EV_CLOSE: {
				if ( ( this->m_connection->flags & MG_F_USER_2 ) == MG_F_USER_2 ) {
					// A failure event has already been dispatched.
				} else if ( ( this->m_connection->flags & MG_F_USER_1 ) == MG_F_USER_1 ) {
					this->m_func( *this, Event::CLOSE, "" );
				} else {
					this->m_func( *this, Event::DROPPED, "" );
				}
				this->m_connection->user_data = NULL;
				this->m_openMutex.unlock();

				// For incoming requests we're responsible for cleaning up the scheduler which was created when the
				// connection was first accepted.
				if ( ( this->m_flags & NETWORK_CONNECTION_FLAG_REQUEST ) == NETWORK_CONNECTION_FLAG_REQUEST ) {
					this->m_flags |= NETWORK_CONNECTION_FLAG_JOINED;
					delete this;
				}

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

		// Everything related to Mongoose, except for mg_broadcast, needs to be executed on the same thread. Therefore
		// we're using a queue to process tasks in between calls to mg_mgr_poll. The poller is interrupted by a call
		// to mg_broadcast after a new task is scheduled to have it executed immediately.
		this->m_worker = std::thread( [this]() -> void {
			do {
				mg_mgr_poll( &this->m_manager, 5000 );

				std::unique_lock<std::recursive_mutex> tasksLock( this->m_tasksMutex );
				while( ! this->m_tasks.empty() ) {
					auto task = this->m_tasks.front();
					this->m_tasks.pop();
					task();
				}
			} while( ! this->m_shutdown );
		} );
	};
	
	Network::~Network() {
		this->m_shutdown = true;
		Network::_wakeUp();
		this->m_worker.join();
		mg_mgr_free( &this->m_manager ); // closes and deallocates all active connections
	};

	std::shared_ptr<Network::Connection> Network::bind( const std::string& port_, Connection::t_eventFunc&& func_ ) {
		std::promise<mg_connection*> promise;
		std::future<mg_connection*> future = promise.get_future();
		Network::_synchronize( [&promise,port_]() {
			Network& network = Network::get();
#ifdef _DEBUG
			assert( ! network.m_shutdown && "Network should be operative before binding." );
#endif // _DEBUG
			promise.set_value( mg_bind( &network.m_manager, port_.c_str(), micasa_mg_handler ) );
		} );
		return std::make_shared<Network::Connection>( future.get(), std::move( func_ ), NETWORK_CONNECTION_FLAG_BIND | NETWORK_CONNECTION_FLAG_HTTP );
	};

#ifdef _WITH_OPENSSL	
	std::shared_ptr<Network::Connection> Network::bind( const std::string& port_, const std::string& cert_, const std::string& key_, Connection::t_eventFunc&& func_ ) {
		std::promise<mg_connection*> promise;
		std::future<mg_connection*> future = promise.get_future();
		Network::_synchronize( [&promise,port_,cert_,key_]() {
			Network& network = Network::get();
#ifdef _DEBUG
			assert( ! network.m_shutdown && "Network should be operative before binding." );
#endif // _DEBUG
			mg_bind_opts options;
			memset( &options, 0, sizeof( options ) );
			if (
				cert_.size() > 0
				&& key_.size() > 0
			) {
				options.ssl_cert = cert_.c_str();
				options.ssl_key = key_.c_str();
			}
			promise.set_value( mg_bind_opt( &network.m_manager, port_.c_str(), micasa_mg_handler, options ) );
		} );
		return std::make_shared<Network::Connection>( future.get(), std::move( func_ ), NETWORK_CONNECTION_FLAG_BIND | NETWORK_CONNECTION_FLAG_HTTP );
	};
#endif

	std::shared_ptr<Network::Connection> Network::connect( const std::string& uri_, Network::Connection::t_eventFunc&& func_ ) {
		std::promise<mg_connection*> promise;
		std::future<mg_connection*> future = promise.get_future();
		bool http = ( uri_.substr( 0, 4 ) == "http" );
		Network::_synchronize( [&promise,uri_,http]() {
			Network& network = Network::get();
#ifdef _DEBUG
			assert( ! network.m_shutdown && "Network should be operative before connecting." );
#endif // _DEBUG
			if ( http ) {
				promise.set_value( mg_connect_http( &network.m_manager, micasa_mg_handler, uri_.c_str(), NULL, NULL ) );
			} else {
				promise.set_value( mg_connect( &network.m_manager, uri_.c_str(), micasa_mg_handler ) );
			}
		} );
		if ( http ) {
			return std::make_shared<Network::Connection>( future.get(), std::move( func_ ), NETWORK_CONNECTION_FLAG_HTTP );
		} else {
			return std::make_shared<Network::Connection>( future.get(), std::move( func_ ), 0 );
		}
	};

	std::shared_ptr<Network::Connection> Network::connect( const std::string& uri_, const std::string& data_, Network::Connection::t_eventFunc&& func_ ) {
		std::promise<mg_connection*> promise;
		std::future<mg_connection*> future = promise.get_future();
		Network::_synchronize( [&promise,uri_,data_]() {
			Network& network = Network::get();
#ifdef _DEBUG
			assert( ! network.m_shutdown && "Network should be operative before connecting." );
#endif // _DEBUG
			promise.set_value( mg_connect_http( &network.m_manager, micasa_mg_handler, uri_.c_str(), "Content-Type: application/json\r\n", data_.c_str() ) );
		} );
		return std::make_shared<Network::Connection>( future.get(), std::move( func_ ), NETWORK_CONNECTION_FLAG_HTTP );
	};

	void Network::_synchronize( std::function<void(void)>&& task_ ) {
		Network& network = Network::get();
		std::unique_lock<std::recursive_mutex> tasksLock( network.m_tasksMutex );
		network.m_tasks.push( std::move( task_ ) );
		tasksLock.unlock();
		Network::_wakeUp();
	};

	void Network::_wakeUp() {
		std::thread( []() { // mg_broadcast needs to be called in a separate thread from mg_mgr_poll
			Network& network = Network::get();
			mg_broadcast( &network.m_manager, micasa_mg_handler, (void*)"", 0 );
		} ).detach();
	};

}; // namespace micasa
