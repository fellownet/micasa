// http://www.vinaysahni.com/best-practices-for-a-pragmatic-restful-api
// https://docs.cesanta.com/mongoose/master/
// https://github.com/Gregwar/mongoose-cpp/blob/master/mongoose/Server.cpp
// http://stackoverflow.com/questions/7698488/turn-a-simple-socket-into-an-ssl-socket
// https://github.com/cesanta/mongoose/blob/master/examples/simplest_web_server/simplest_web_server.c

#include "WebServer.h"

#include "json.hpp"

extern "C" {
	
	static void mongoose_handler( mg_connection* connection_, int event_, void* data_ ) {
		micasa::WebServer* webserver = reinterpret_cast<micasa::WebServer*>( connection_->user_data );
		if ( event_ == MG_EV_HTTP_REQUEST ) {
			webserver->_processHttpRequest( connection_, (http_message*)data_ );
		}
	}
	
} // extern "C"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;
	
	WebServer::WebServer() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before global WebServer instance." );
		assert( g_logger && "Global Logger instance should be created before global WebServer instance." );
#endif // _DEBUG
	};

	WebServer::~WebServer() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after global WebServer instance." );
		assert( g_logger && "Global Logger instance should be destroyed after global WebServer instance." );
		assert( this->m_resources.size() == 0 && "All resources should be removed before the global WebServer instance is destroyed." );
#endif // _DEBUG
	};

	std::string WebServer::toString() const {
		return "WebServer";
	};

	void WebServer::addResourceHandler( std::string resource_, int supportedMethods_, std::shared_ptr<WebServerResource> handler_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources["api/" + resource_] = std::pair<int, std::shared_ptr<WebServerResource> >( supportedMethods_, handler_ );
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Resource no. %d at " + resource_ + " installed.", this->m_resources.size() );
	}

	void WebServer::removeResourceHandler( std::string resource_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.erase( "api/" + resource_ );
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Resource " + resource_ + " removed, %d resources left.", this->m_resources.size() );
	}

	void WebServer::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		
		mg_mgr_init( &this->m_manager, NULL );

		mg_bind_opts bind_opts;
		memset( &bind_opts, 0, sizeof( bind_opts ) );
		bind_opts.user_data = this;
#ifdef _WITH_SSL
		bind_opts.ssl_cert = "server.pem";
		bind_opts.ssl_key = "key.pem";
		this->m_connection = mg_bind_opt( &this->m_manager, "443", mongoose_handler, bind_opts );
#endif // _WITH_SSL
#ifndef _WITH_SSL
		this->m_connection = mg_bind_opt( &this->m_manager, "8081", mongoose_handler, bind_opts );
#endif // _WITH_SSL

		if ( false == this->m_connection ) {
			// TODO can we be a little bit more descriptive about the error?
			g_logger->log( Logger::LogLevel::ERROR, this, "Unable to bind webserver. Port in use or user permission issue?" );
			return;
		}
		
		mg_set_protocol_http_websocket( this->m_connection );
		mg_enable_multithreading( this->m_connection );

		this->_begin();
		
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void WebServer::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->_retire();
		mg_mgr_free( &this->m_manager );
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::chrono::milliseconds WebServer::_work( unsigned long int iteration_ ) {
		mg_mgr_poll( &this->m_manager, 100 );
		return std::chrono::milliseconds( 0 );
	}
	
	void WebServer::_processHttpRequest( mg_connection* connection_, http_message* message_ ) {
		
		// Determine method.
		std::string methodStr = "";
		methodStr.assign( message_->method.p, message_->method.len );
		WebServerResource::Method method = WebServerResource::GET;
		if ( methodStr == "HEAD" ) {
			method = WebServerResource::HEAD;
		} else if ( methodStr == "POST" ) {
			method = WebServerResource::POST;
		} else if ( methodStr == "PUT" ) {
			method = WebServerResource::PUT;
		} else if ( methodStr == "PATCH" ) {
			method = WebServerResource::PATCH;
		} else if ( methodStr == "DELETE" ) {
			method = WebServerResource::DELETE;
		} else if ( methodStr == "OPTIONS" ) {
			method = WebServerResource::OPTIONS;
		}

		// Determine resource (the leading / is stripped, our resources start without it).
		std::string uriStr = "";
		if ( message_->uri.len >= 1 ) {
			uriStr.assign( message_->uri.p + 1, message_->uri.len - 1 );
		}
		
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		
		std::string result;
		std::string contentType;
		int code = 200;
		
		if ( this->m_resources.find( uriStr ) != this->m_resources.end() ) {
			auto resource = this->m_resources[uriStr];
			if ( ( resource.first & method ) == method ) {
				
				std::map<std::string, std::string> poe;
				
				resource.second->handleRequest( uriStr, method, poe );
			}
			
			result = "resource moet dit vullen";
			contentType = "Content-Type: text/plain";
			
		} else if ( uriStr == "api" ) {
			
			std::stringstream resultStream;
			resultStream << "<!doctype html>" << "<html><body>";
			for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
				resultStream << "<strong>Uri:</strong> <a href=\"/" << resourceIt->first << "\">" << resourceIt->first << "</a><br>";
				resultStream << "<strong>Methods:</strong>";
				if ( ( resourceIt->second.first & WebServerResource::GET ) == WebServerResource::GET ) {
					resultStream << " GET";
				}
				if ( ( resourceIt->second.first & WebServerResource::HEAD ) == WebServerResource::HEAD ) {
					resultStream << " HEAD";
				}
				if ( ( resourceIt->second.first & WebServerResource::POST ) == WebServerResource::POST ) {
					resultStream << " POST";
				}
				if ( ( resourceIt->second.first & WebServerResource::PUT ) == WebServerResource::PUT ) {
					resultStream << " PUT";
				}
				if ( ( resourceIt->second.first & WebServerResource::PATCH ) == WebServerResource::PATCH ) {
					resultStream << " PATCH";
				}
				if ( ( resourceIt->second.first & WebServerResource::DELETE ) == WebServerResource::DELETE ) {
					resultStream << " DELETE";
				}
				if ( ( resourceIt->second.first & WebServerResource::OPTIONS ) == WebServerResource::OPTIONS ) {
					resultStream << " OPTIONS";
				}
				
				resultStream << "<br><br>";
			}
			resultStream << "</body></html>";
			
			
			result = resultStream.str();
			contentType = "Content-Type: text/html";
			
		} else {
		
			mg_serve_http_opts options;
			memset( &options, 0, sizeof( options ) );
			options.document_root = "www";
			mg_serve_http( connection_, message_, options );
			return;
			
			
			
			// This is an invalid API resource, show an error.
			json resultJson;
			resultJson["code"] = 404;
			resultJson["message"] = "The requested resource was not found.";
			result = resultJson.dump();
			contentType = "Content-Type: application/json";
			code = 404;
		}
 
		mg_send_head( connection_, code, result.length(), contentType.c_str() );
		mg_send( connection_, result.c_str(), result.length() );
		connection_->flags |= MG_F_SEND_AND_CLOSE;
	}

}; // namespace micasa
