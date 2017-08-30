#include <regex>
#include <sstream>

#include "Network.h"
#include "Logger.h"

void micasa_mg_handler( mg_connection* mg_conn_, int event_, void* data_ ) {
	micasa::Network::get()._handler( mg_conn_, event_, data_ );
}

namespace micasa {

	using namespace nlohmann;

	// ==========
	// Connection
	// ==========

	Network::Connection::Connection( mg_connection* mg_conn_, unsigned int flags_, t_eventFunc&& func_ ) :
		m_mg_conn( mg_conn_ ),
		m_flags( flags_ ),
		m_func( std::move( func_ ) )
	{
	};

	Network::Connection::Connection( mg_connection* mg_conn_, unsigned int flags_, const t_eventFunc& func_ ) :
		m_mg_conn( mg_conn_ ),
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
		std::unique_lock<std::mutex> lock( this->m_mutex );
		this->m_tasks.push( [this,root_,index_]() {
			mg_serve_http_opts options;
			memset( &options, 0, sizeof( options ) );
			options.document_root = root_.c_str();
			options.index_files = index_.c_str();
			options.enable_directory_listing = "no";
			mg_serve_http( this->m_mg_conn, &this->m_http, options );
		} );
		lock.unlock();
		mg_broadcast( &Network::get().m_manager, micasa_mg_handler, (void*)"", 0 );
	};

	void Network::Connection::reply( const std::string& data_, int code_, const std::map<std::string, std::string>& headers_, bool close_ ) {
		std::unique_lock<std::mutex> lock( this->m_mutex );
		this->m_tasks.push( [this,data_,code_,headers_,close_]() {
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
				if ( close_ ) {
					this->m_mg_conn->flags |= MG_F_SEND_AND_CLOSE;
					this->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
				}
			}
		} );
		lock.unlock();
		mg_broadcast( &Network::get().m_manager, micasa_mg_handler, (void*)"", 0 );
	};

	void Network::Connection::send( const std::string& data_ ) {
		std::unique_lock<std::mutex> lock( this->m_mutex );
		this->m_tasks.push( [this,data_]() {
			if ( this->m_mg_conn != nullptr ) {
				if ( ( this->m_flags & NETWORK_CONNECTION_FLAG_SOCKET ) == NETWORK_CONNECTION_FLAG_SOCKET ) {
					mg_send_websocket_frame( this->m_mg_conn, WEBSOCKET_OP_TEXT, data_.c_str(), data_.length() );
				} else {
					mg_send( this->m_mg_conn, data_.c_str(), data_.length() );
				}
			}
		} );
		lock.unlock();
		mg_broadcast( &Network::get().m_manager, micasa_mg_handler, (void*)"", 0 );
	};

	std::string Network::Connection::getData() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		return this->m_data;
	};

	std::string Network::Connection::popData( unsigned int length_ ) {
		unsigned int length = length_ > this->m_data.size() ? this->m_data.size() : length_;
		std::string result = this->m_data.substr( 0, length );
		this->m_data.erase( 0, length );
		return result;
	};

	std::string Network::Connection::getBody() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		return std::string( this->m_http.body.p, this->m_http.body.len );
	};

	std::string Network::Connection::getUri() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		return std::string( this->m_http.uri.p, this->m_http.uri.len );
	};

	unsigned int Network::Connection::getPort() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		char* buffer = new char[6]; // max 65535 length  5 + \0
		mg_conn_addr_to_str( this->m_mg_conn, buffer, 6, MG_SOCK_STRINGIFY_PORT );
		return std::stoi( buffer );
	};

	std::string Network::Connection::getIp() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		char* buffer = new char[46]; // max ipv6 length = 45 + \0
		mg_conn_addr_to_str( this->m_mg_conn, buffer, 46, MG_SOCK_STRINGIFY_IP );
		return std::string( buffer );
	};

	std::string Network::Connection::getQuery() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		return std::string( this->m_http.query_string.p, this->m_http.query_string.len );
	};

	std::string Network::Connection::getMethod() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		return std::string( this->m_http.method.p, this->m_http.method.len );
	};

	std::map<std::string, std::string> Network::Connection::getHeaders() const {
		std::lock_guard<std::mutex> lock( this->m_mutex );
		std::map<std::string, std::string> headers;
		int i = 0;
		for ( const mg_str& name : this->m_http.header_names ) {
			std::string header, value;
			header.assign( name.p, name.len );
			if ( ! header.empty() ) {
				value.assign( this->m_http.header_values[i].p, this->m_http.header_values[i].len );
				headers[header] = value;
			}
			i++;
		}
		return headers;
	};

	std::map<std::string, std::string> Network::Connection::getParams() const {
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
				mg_mgr_poll( &this->m_manager, 1000 );
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
		Logger::logr( Logger::LogLevel::VERBOSE, &network, "Binding to %s.", port_.c_str() );
		std::shared_ptr<Connection> connection;
		if ( mg_conn ) {
			mg_set_protocol_http_websocket( mg_conn );
			connection = std::make_shared<Connection>( mg_conn, NETWORK_CONNECTION_FLAG_HTTP | NETWORK_CONNECTION_FLAG_BIND, std::move( func_ ) );
			mg_conn->user_data = mg_conn;
			network.m_connections.insert( { mg_conn, connection } );
			return connection;
		} else {
			return nullptr;
		}
	};

	std::shared_ptr<Network::Connection> Network::_connect( const std::string& uri_, const nlohmann::json& data_, Network::Connection::t_eventFunc&& func_ ) {
		Network& network = Network::get();
		mg_connection* mg_conn;
		unsigned int flags = 0;
		if ( __likely( uri_.substr( 0, 4 ) == "http" ) ) {
			if ( data_.is_null() ) {
				mg_conn = mg_connect_http( &network.m_manager, micasa_mg_handler, uri_.c_str(), NULL, NULL );
			} else {
				mg_conn = mg_connect_http( &network.m_manager, micasa_mg_handler, uri_.c_str(), "Content-Type: application/json\r\n", data_.dump().c_str() );
			}
			if ( mg_conn ) {
				mg_set_protocol_http_websocket( mg_conn );
				flags |= NETWORK_CONNECTION_FLAG_HTTP;
			}
		} else {
			mg_conn = mg_connect( &network.m_manager, uri_.c_str(), micasa_mg_handler );
		}
		if ( mg_conn ) {
			Logger::logr( Logger::LogLevel::VERBOSE, &network, "Connecting to %s.", uri_.c_str() );
			mg_set_timer( mg_conn, mg_time() + NETWORK_CONNECTION_DEFAULT_TIMEOUT_SEC );
			std::shared_ptr<Connection> connection = std::make_shared<Connection>( mg_conn, flags, std::move( func_ ) );
			network.m_connections.insert( { mg_conn, connection } );
			return connection;
		} else {
			return nullptr;
		}
	};

	inline void Network::_handler( mg_connection* mg_conn_, int event_, void* data_ ) {
		Network& network = Network::get();

		std::shared_ptr<Connection> connection = nullptr;
		if ( event_ == MG_EV_ACCEPT ) {
			auto find = network.m_connections.find( (mg_connection*)mg_conn_->user_data );
			if ( find != network.m_connections.end() ) {
				connection = std::make_shared<Connection>( mg_conn_, find->second->m_flags & ~NETWORK_CONNECTION_FLAG_BIND, find->second->m_func );
				network.m_connections.insert( { mg_conn_, connection } );
				Logger::logr( Logger::LogLevel::VERBOSE, &network, "Accepted connection from %s.", connection->getIp().c_str() );
			}
		} else {
			auto find = network.m_connections.find( mg_conn_ );
			if ( find != network.m_connections.end() ) {
				connection = find->second;
			}
		}

		if ( connection ) {
			switch( event_ ) {
				case MG_EV_POLL: {
					std::unique_lock<std::mutex> lock( connection->m_mutex );
					while( ! connection->m_tasks.empty() ) {
						if ( connection->m_mg_conn ) {
							connection->m_tasks.front()();
						}
						connection->m_tasks.pop();
					}
					lock.unlock();
					break;
				}

				case MG_EV_ACCEPT: {
					if ( connection->m_func != nullptr ) {
						network.m_scheduler.schedule( 0, 1, &network, [connection]( std::shared_ptr<Scheduler::Task<>> ) {
							connection->m_func( connection, Connection::Event::CONNECT );
						} );
					}
					break;
				}

				case MG_EV_CONNECT: {
					mg_set_timer( connection->m_mg_conn, 0 );
					int status = *(int*)data_;
					Connection::Event event = Connection::Event::CONNECT;
					if ( status != 0 ) {
						connection->m_flags |= NETWORK_CONNECTION_FLAG_FAILURE;
						event = Connection::Event::FAILURE;
					}
					if ( connection->m_func != nullptr ) {
						network.m_scheduler.schedule( 0, 1, &network, [connection,event]( std::shared_ptr<Scheduler::Task<>> ) {
							connection->m_func( connection, event );
						} );
					}
					break;
				}

				case MG_EV_TIMER: {
					connection->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
					connection->m_flags |= NETWORK_CONNECTION_FLAG_FAILURE;
					if ( connection->m_func != nullptr ) {
						network.m_scheduler.schedule( 0, 1, &network, [connection]( std::shared_ptr<Scheduler::Task<>> ) {
							connection->m_func( connection, Connection::Event::FAILURE );
						} );
					}
					break;
				}

				case MG_EV_RECV: {
					if ( ( connection->m_flags & NETWORK_CONNECTION_FLAG_HTTP ) == 0 ) {
						connection->m_data += std::string( connection->m_mg_conn->recv_mbuf.buf, connection->m_mg_conn->recv_mbuf.len );
						mbuf_remove( &connection->m_mg_conn->recv_mbuf, connection->m_mg_conn->recv_mbuf.len );
						if ( connection->m_func != nullptr ) {
							network.m_scheduler.schedule( 0, 1, &network, [connection]( std::shared_ptr<Scheduler::Task<>> ) {
								connection->m_func( connection, Connection::Event::DATA );
							} );
						}
					}
					break;
				}

				case MG_EV_HTTP_REQUEST:
				case MG_EV_HTTP_REPLY:
				case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST: {
					connection->m_http = *(http_message*)data_;
					if ( event_ == MG_EV_HTTP_REPLY ) {
						connection->m_mg_conn->flags |= MG_F_CLOSE_IMMEDIATELY;
						connection->m_flags |= NETWORK_CONNECTION_FLAG_CLOSE;
					}
					if ( connection->m_func != nullptr ) {
						network.m_scheduler.schedule( 0, 1, &network, [connection]( std::shared_ptr<Scheduler::Task<>> ) {
							connection->m_func( connection, Connection::Event::HTTP );
						} );
					}
					break;
				}

				case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
					connection->m_flags |= NETWORK_CONNECTION_FLAG_SOCKET;
					break;
				}

				case MG_EV_CLOSE: {
					if ( ( connection->m_flags & NETWORK_CONNECTION_FLAG_FAILURE ) == 0 ) {
						Connection::Event event = Connection::Event::DROPPED;
						if ( ( connection->m_flags & NETWORK_CONNECTION_FLAG_CLOSE ) == NETWORK_CONNECTION_FLAG_CLOSE ) {
							event = Connection::Event::CLOSE;
						}
						if ( connection->m_func != nullptr ) {
							network.m_scheduler.schedule( 0, 1, &network, [connection,event]( std::shared_ptr<Scheduler::Task<>> ) {
								connection->m_func( connection, event );
							} );
						}
					}
					network.m_connections.erase( connection->m_mg_conn );
					connection->m_mg_conn = nullptr;
				}
			}
		}
	};

}; // namespace micasa
