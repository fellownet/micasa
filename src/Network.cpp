// mongoose
// http://www.vinaysahni.com/best-practices-for-a-pragmatic-restful-api
// https://docs.cesanta.com/mongoose/master/
// https://github.com/Gregwar/mongoose-cpp/blob/master/mongoose/Server.cpp
// http://stackoverflow.com/questions/7698488/turn-a-simple-socket-into-an-ssl-socket
// https://github.com/cesanta/mongoose/blob/master/examples/simplest_web_server/simplest_web_server.c

#include "Network.h"

#include <memory>
#include <chrono>
#include <iostream>

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

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
	
	extern std::shared_ptr<Logger> g_logger;
	
	Network::Network() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be created before global Network instance." );
#endif // _DEBUG
	};
	
	Network::~Network() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be destroyed after global Network instance." );
#endif // _DEBUG
	};
	
	void Network::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		mg_mgr_init( &this->m_manager, NULL );
		Worker::start(); // starts polling on the manager
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void Network::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Worker::stop(); // makes sure the last _work call is finished
		mg_mgr_free( &this->m_manager ); // disconnects all open connections (with the MG_EV_SHUTDOWN event)
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};
	
	const std::chrono::milliseconds Network::_work( const unsigned long int& iteration_ ) {
		// The _work method is executed in the network worker thread, and so will *all* the installed callbacks. The
		// connect and bind methods are synchronized with this worker thread. Because these methods are usually called
		// at the very beginning of the app lifecycle, skip blocking the first iteration.
		if ( iteration_ == 1 ) {
			return std::chrono::milliseconds( 250 );
		} else {
			mg_mgr_poll( &this->m_manager, 250 );
			return std::chrono::milliseconds( 0 );
		}
	}
	
	mg_connection* Network::bind( const std::string port_, const Network::t_callback callback_ ) {
		return this->bind( port_, "", "", callback_ );
	};
	
	mg_connection* Network::bind( const std::string port_, const std::string cert_, const std::string key_, const Network::t_callback callback_ ) {
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "Binding to port %s.", port_.c_str() );
		mg_bind_opts options;
		memset( &options, 0, sizeof( options ) );
#ifdef _WITH_SSL
		if (
			cert_.size() > 0
			&& key_.size() > 0
		) {
			options.ssl_cert = cert_.c_str();
			options.ssl_key = key_.c_str();
		}
#endif // _WITH_SSL
		
		mg_connection* connection;
		this->_synchronize( [&]() {
			connection = mg_bind_opt( &this->m_manager, port_.c_str(), micasa_mg_handler, options );
			if ( NULL != connection ) {
				connection->user_data = this->_newHandler( callback_, true );
				mg_set_protocol_http_websocket( connection );
				this->m_bindings.insert( connection );
			}
		} );
		return connection;
	};
	
	mg_connection* Network::connect( const std::string uri_, const Network::t_callback callback_ ) {
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "Connecting to %s.", uri_.c_str() );
		mg_connection* connection;
		this->_synchronize( [&]() {
			if ( uri_.substr( 0, 4 ) == "http" ) {
				connection = mg_connect_http( &this->m_manager, micasa_mg_handler, uri_.c_str(), NULL, NULL );
			} else {
				connection = mg_connect( &this->m_manager, uri_.c_str(), micasa_mg_handler );
			}
			if ( NULL != connection ) {
				connection->user_data = this->_newHandler( callback_, false );
			}
		} );
		return connection;
	};
	
	Network::t_handler* Network::_newHandler( const Network::t_callback callback_, const bool binding_ ) {
		return new Network::t_handler( [this, callback_, binding_]( mg_connection* connection_, int event_, void* data_ ) {
			// If the network is shutting down, all connections are force closed. To give all open connections the
			// ability to distinguish between a failty close and a shutdown we're rewriting the event.
			if (
				event_ == MG_EV_CLOSE
				&& this->m_shutdown
			) {
				event_ = MG_EV_SHUTDOWN;
			}

			callback_( connection_, event_, data_ );
			
			// Bindings spwam new connection for each client. These cannot cause the handler to be freed, so we need
			// to check if it's the binding connection or a client connection that is closed.
			if (
				event_ == MG_EV_CLOSE
				|| event_ == MG_EV_SHUTDOWN
			) {
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
