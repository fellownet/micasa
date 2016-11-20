// mongoose
// http://www.vinaysahni.com/best-practices-for-a-pragmatic-restful-api
// https://docs.cesanta.com/mongoose/master/
// https://github.com/Gregwar/mongoose-cpp/blob/master/mongoose/Server.cpp
// http://stackoverflow.com/questions/7698488/turn-a-simple-socket-into-an-ssl-socket
// https://github.com/cesanta/mongoose/blob/master/examples/simplest_web_server/simplest_web_server.c

// json
// https://github.com/nlohmann/json

#include <cstdlib>

#include "WebServer.h"

#include "json.hpp"

extern "C" {

	static void mongoose_handler( mg_connection* connection_, int event_, void* data_ ) {
		// NOTE do not block this; store the connection instead for later use (long polling?) as adviced
		// by an engineer of mongoose (multithreading might be obsoleted in the future due to it being
		// very error prone).
		micasa::WebServer* webserver = reinterpret_cast<micasa::WebServer*>( connection_->user_data );
		if ( event_ == MG_EV_HTTP_REQUEST ) {
			webserver->_processHttpRequest( connection_, (http_message*)data_ );
		}
	}

} // extern "C"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;

	WebServer::WebServer() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be created before global WebServer instance." );
#endif // _DEBUG
		srand ( time( NULL ) );
	};

	WebServer::~WebServer() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be destroyed after global WebServer instance." );
		assert( this->m_resources.size() == 0 && "All resources should be removed before the global WebServer instance is destroyed." );
#endif // _DEBUG
	};

	std::string WebServer::toString() const {
		return "WebServer";
	};

	void WebServer::addResource( Resource resource_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		std::string etag = std::to_string( rand() );
		this->m_resources[resource_.uri] = std::pair<Resource, std::string>( resource_, etag );
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Resource no. %d at " + resource_.uri + " installed.", this->m_resources.size() );
	};
	
	void WebServer::removeResourceAt( std::string uri_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
#ifdef _DEBUG
		assert( this->m_resources.find( uri_ ) != this->m_resources.end() && "Resource should exist before trying to remove it." );
#endif // _DEBUG
		this->m_resources.erase( uri_ );
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Resource " + uri_ + " removed, %d resources left.", this->m_resources.size() );
	};
	
	void WebServer::touchResourceAt( std::string uri_ ) {
#ifdef _DEBUG
		assert( this->m_resources.find( uri_ ) != this->m_resources.end() && "Resource should exist before trying to touch it." );
#endif // _DEBUG
		std::string etag = std::to_string( rand() );
		this->m_resources[uri_].second = etag;
	};

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
			g_logger->log( Logger::LogLevel::ERROR, this, "Unable to bind webserver. Port in use, user permission issue or certificate problem?" );
			return;
		}

		mg_set_protocol_http_websocket( this->m_connection );

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
		ResourceMethod method = ResourceMethod::GET;
		if ( methodStr == "HEAD" ) {
			method = ResourceMethod::HEAD;
		} else if ( methodStr == "POST" ) {
			method = ResourceMethod::POST;
		} else if ( methodStr == "PUT" ) {
			method = ResourceMethod::PUT;
		} else if ( methodStr == "PATCH" ) {
			method = ResourceMethod::PATCH;
		} else if ( methodStr == "DELETE" ) {
			method = ResourceMethod::DELETE;
		} else if ( methodStr == "OPTIONS" ) {
			method = ResourceMethod::OPTIONS;
		}

		// Determine resource (the leading / is stripped, our resources start without it) and see if
		// a resource handler has been added for it.
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		std::string uriStr = "";
		if ( message_->uri.len >= 1 ) {
			uriStr.assign( message_->uri.p + 1, message_->uri.len - 1 );
		}
		auto resource = this->m_resources.find( uriStr );
		if (
			resource != this->m_resources.end()
			&& ( resource->second.first.methods & method ) == method
		) {
			
			std::string etag;
			int i = 0;
			for ( const mg_str &headerName : message_->header_names ) {
				std::string header;
				header.assign( headerName.p, headerName.len );
				if ( header == "If-None-Match" ) {
					etag.assign( message_->header_values[i].p, message_->header_values[i].len );
				}
				i++;
			}
			if ( etag == resource->second.second ) {
				
				// Resource here is the same as client, send not-changed header (=304).
				// TODO test this with ajax calls (in relation to a missing access-control header
				// when caching).
				mg_send_head( connection_, 304, 0, "" );
				mg_send( connection_, "", 0 );
				connection_->flags |= MG_F_SEND_AND_CLOSE;
				
			} else {
				
				// We've got a newer version of the resource here, send it to the client.
				json outputJson;
				int code = 200;
				resource->second.first.handler->handleResource( resource->second.first, code, outputJson );

				std::string output = outputJson.dump( 4 );
				std::stringstream headers;
#ifdef _DEBUG
				headers << "Content-Type: Content-type: text/plain\r\n";
#endif // _DEBUG
#ifndef _DEBUG
				headers << "Content-Type: Content-type: application/json\r\n";
#endif // _DEBUG
				headers << "Access-Control-Allow-Origin: *\r\n";
				headers << "ETag: " << resource->second.second;
				
				mg_send_head( connection_, 200, output.length(), headers.str().c_str() );
				mg_send( connection_, output.c_str(), output.length() );
				connection_->flags |= MG_F_SEND_AND_CLOSE;
			}
			
		} else if (
			uriStr == "api"
			&& method == ResourceMethod::GET
		) {
			
			// Build a standard documentation page if the api uri is requested directly using get.
			std::stringstream outputStream;
			outputStream << "<!doctype html>" << "<html><body style=\"font-family:sans-serif;\">";
			for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
				outputStream << "<strong>Title:</strong> " << resourceIt->second.first.title << "<br>";
				outputStream << "<strong>Uri:</strong> <a href=\"/" << resourceIt->first << "\">" << resourceIt->first << "</a><br>";
				outputStream << "<strong>Methods:</strong>";
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::GET ) == WebServer::ResourceMethod::GET ) {
					outputStream << " GET";
				}
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::HEAD ) == WebServer::ResourceMethod::HEAD ) {
					outputStream << " HEAD";
				}
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::POST ) == WebServer::ResourceMethod::POST ) {
					outputStream << " POST";
				}
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::PUT ) == WebServer::ResourceMethod::PUT ) {
					outputStream << " PUT";
				}
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::PATCH ) == WebServer::ResourceMethod::PATCH ) {
					outputStream << " PATCH";
				}
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::DELETE ) == WebServer::ResourceMethod::DELETE ) {
					outputStream << " DELETE";
				}
				if ( ( resourceIt->second.first.methods & WebServer::ResourceMethod::OPTIONS ) == WebServer::ResourceMethod::OPTIONS ) {
					outputStream << " OPTIONS";
				}
				outputStream << "<br><br>";
			}
			
			std::string output = outputStream.str();
			mg_send_head( connection_, 200, output.length(), "Content-Type: text/html" );
			mg_send( connection_, output.c_str(), output.length() );
			connection_->flags |= MG_F_SEND_AND_CLOSE;
			
		} else {
			
			// If some other resource was accessed, serve static files instead.
			// TODO caching implementation
			// TODO gzip too?
			// TODO better 404 handling?
			mg_serve_http_opts options;
			memset( &options, 0, sizeof( options ) );
			options.document_root = "www";
			options.enable_directory_listing = "no";
			mg_serve_http( connection_, message_, options );
		}
	}

}; // namespace micasa
