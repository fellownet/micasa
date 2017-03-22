// mongoose
// http://www.vinaysahni.com/best-practices-for-a-pragmatic-restful-api
// https://docs.cesanta.com/mongoose/master/
// https://github.com/Gregwar/mongoose-cpp/blob/master/mongoose/Server.cpp
// http://stackoverflow.com/questions/7698488/turn-a-simple-socket-into-an-ssl-socket
// https://github.com/cesanta/mongoose/blob/master/examples/simplest_web_server/simplest_web_server.c

#include <memory>
#include <chrono>
#include <iostream>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Network.h"

extern "C" {
	void micasa_mg_handler( mg_connection* connection_, int event_, void* data_ ) {
		if ( MG_EV_POLL != event_ ) {
			micasa::Network::t_handler* handler = reinterpret_cast<micasa::Network::t_handler*>( connection_->user_data );
			bool free = (*handler)( connection_, event_, data_ );
			if ( free ) {
				delete( handler );
			}
		}
	}
} // extern "C"

namespace micasa {
	
	using namespace nlohmann;

	Network::Network() {
		mg_mgr_init( &this->m_manager, NULL );
		this->m_worker = std::thread( [this]() -> void {
			while( ! this->m_shutdown ) {
				mg_mgr_poll( &this->m_manager, 250 );
			}
		} );
	};
	
	Network::~Network() {
		this->m_shutdown = true;
		this->m_worker.join();
		mg_mgr_free( &this->m_manager );
	};
	
	mg_connection* Network::bind( const std::string port_, const Network::t_callback callback_ ) {
		mg_connection* connection = mg_bind( &this->m_manager, port_.c_str(), micasa_mg_handler );
		if ( NULL != connection ) {
			connection->user_data = this->_newHandler( callback_, true );
			mg_set_protocol_http_websocket( connection );
			this->m_bindings.insert( connection );
		}
		return connection;
	};

#ifdef _WITH_OPENSSL	
	mg_connection* Network::bind( const std::string port_, const std::string cert_, const std::string key_, const Network::t_callback callback_ ) {
		mg_bind_opts options;
		memset( &options, 0, sizeof( options ) );
		if (
			cert_.size() > 0
			&& key_.size() > 0
		) {
			options.ssl_cert = cert_.c_str();
			options.ssl_key = key_.c_str();
		}

		mg_connection* connection = mg_bind_opt( &this->m_manager, port_.c_str(), micasa_mg_handler, options );
		if ( NULL != connection ) {
			connection->user_data = this->_newHandler( callback_, true );
			mg_set_protocol_http_websocket( connection );
			this->m_bindings.insert( connection );
		}
		return connection;
	};
#endif

	mg_connection* Network::connect( const std::string uri_, const Network::t_callback callback_ ) {
		mg_connection* connection;
		if ( uri_.substr( 0, 4 ) == "http" ) {
			connection = mg_connect_http( &this->m_manager, micasa_mg_handler, uri_.c_str(), NULL, NULL );
		} else {
			connection = mg_connect( &this->m_manager, uri_.c_str(), micasa_mg_handler );
		}
		if ( NULL != connection ) {
			connection->user_data = this->_newHandler( callback_, false );
		}
		return connection;
	};

	mg_connection* Network::connect( const std::string uri_, const Network::t_callback callback_, const json& body_ ) {
		mg_connection* connection;
		if ( uri_.substr( 0, 4 ) == "http" ) {
			connection = mg_connect_http( &this->m_manager, micasa_mg_handler, uri_.c_str(), "Content-Type: application/json\r\n", body_.dump().c_str() );
		} else {
			throw std::runtime_error( "invalid protocol" );
		}
		if ( NULL != connection ) {
			connection->user_data = this->_newHandler( callback_, false );
		}
		return connection;
	};


	Network::t_handler* Network::_newHandler( const Network::t_callback callback_, const bool binding_ ) {
		return new Network::t_handler( [this, callback_, binding_]( mg_connection* connection_, int event_, void* data_ ) {
			callback_( connection_, event_, data_ );
			
			// Bindings spwam new connection for each client. These cannot cause the handler to be freed, so we need
			// to check if it's the binding connection or a client connection that is closed.
			if ( event_ == MG_EV_CLOSE ) {
				if ( binding_ ) {
					auto bindingsIt = this->m_bindings.find( connection_ );
					if ( bindingsIt != this->m_bindings.end() ) {
						this->m_bindings.erase( bindingsIt );
						return true;
					}
				} else {
					// All non binding connections (cq. outbound connections) should have their handlers freed when
					// they disconnect.
					return true;
				}
			}

			return false;
		} );
	}
	
}; // namespace micasa
