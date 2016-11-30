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
#include "Database.h"
#include "Logger.h"
#include "Utils.h"

#include "json.hpp"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Network> g_network;
	extern std::shared_ptr<Database> g_database;
	
	using namespace nlohmann;

	WebServer::WebServer() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be created before global WebServer instance." );
		assert( g_logger && "Global Network instance should be created before global WebServer instance." );
		assert( g_logger && "Global Database instance should be created before global WebServer instance." );
#endif // _DEBUG
		srand ( time( NULL ) );
	};

	WebServer::~WebServer() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be destroyed after global WebServer instance." );
		assert( g_logger && "Global Network instance should be destroyed after global WebServer instance." );
		assert( g_logger && "Global Database instance should be destroyed after global WebServer instance." );
		assert( this->m_resources.size() == 0 && "All resources should be removed before the global WebServer instance is destroyed." );
		assert( this->m_resourceCache.size() == 0 && "All resource tags should be removed before the global WebServer instance is destroyed." );
#endif // _DEBUG
	};

	void WebServer::start() {
#ifdef _DEBUG
		assert( g_network->isRunning() && "Network should be running before WebServer is started." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		g_network->bind( "8081", Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
			if ( event_ == MG_EV_HTTP_REQUEST ) {
				this->_processHttpRequest( connection_, (http_message*)data_ );
			}
		} ) );
		
		this->_begin();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void WebServer::stop() {
#ifdef _DEBUG
		assert( g_network->isRunning() && "Network should be running before WebServer is stopped." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		this->_retire();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};
	
	std::chrono::milliseconds WebServer::_work( const unsigned long int iteration_ ) {
		// TODO if it turns out that the webserver instance doesn't need to do stuff periodically, remove the
		// extend from Worker.
		return std::chrono::milliseconds( 5000 );
	};

	void WebServer::addResourceCallback( std::shared_ptr<ResourceCallback> callback_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		auto resourceIt = this->m_resources.find( callback_->uri );
		if ( resourceIt != this->m_resources.end() ) {
			resourceIt->second.push_back( callback_ );
		} else {
			this->m_resources[callback_->uri] = { callback_ };
		}
		this->m_resourceCache.erase( callback_->uri );
#ifdef _DEBUG
		g_logger->logr( Logger::LogLevel::DEBUG, this, "Resource callback installed at %s.", callback_->uri.c_str() );
#endif // _DEBUG
	};
	
	void WebServer::removeResourceCallback( const std::string reference_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); ) {
			for ( auto callbackIt = resourceIt->second.begin(); callbackIt != resourceIt->second.end(); ) {
				if ( (*callbackIt)->reference == reference_ ) {
#ifdef _DEBUG
					g_logger->logr( Logger::LogLevel::DEBUG, this, "Resource callback removed at %s.", (*callbackIt)->uri.c_str() );
#endif // _DEBUG
					callbackIt = resourceIt->second.erase( callbackIt );
					this->m_resourceCache.erase( (*callbackIt)->uri );
				} else {
					callbackIt++;
				}
			}
			if ( resourceIt->second.size() == 0 ) {
				resourceIt = this->m_resources.erase( resourceIt );
			} else {
				resourceIt++;
			}
		}
	};

	void WebServer::touchResourceAt( const std::string uri_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resourceCache.erase( uri_ );
	};
	
	
	
	
	/*
	void WebServer::addResource( Resource resource_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		std::string etag = std::to_string( rand() );
		this->m_resources[resource_.uri] = std::pair<Resource, std::string>( resource_, etag );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "Resource no. %d at " + resource_.uri + " installed.", this->m_resources.size() );
	};
	
	void WebServer::removeResourceAt( std::string uri_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
#ifdef _DEBUG
		assert( this->m_resources.find( uri_ ) != this->m_resources.end() && "Resource should exist before trying to remove it." );
#endif // _DEBUG
		this->m_resources.erase( uri_ );
		g_logger->logr( Logger::LogLevel::VERBOSE, this, "Resource " + uri_ + " removed, %d resources left.", this->m_resources.size() );
	};
	
	void WebServer::touchResourceAt( std::string uri_ ) {
#ifdef _DEBUG
		assert( this->m_resources.find( uri_ ) != this->m_resources.end() && "Resource should exist before trying to touch it." );
#endif // _DEBUG
		std::string etag = std::to_string( rand() );
		this->m_resources[uri_].second = etag;
	};
	*/

	void WebServer::_processHttpRequest( mg_connection* connection_, http_message* message_ ) {
		
		std::string methodStr = "";
		methodStr.assign( message_->method.p, message_->method.len );
		std::string queryStr = "";
		queryStr.assign( message_->query_string.p, message_->query_string.len );
		std::string uriStr = "";
		if ( message_->uri.len >= 1 ) {
			uriStr.assign( message_->uri.p + 1, message_->uri.len - 1 );
		}
		
		// Determine method (note that the method can be overridden by adding _method=xx to the query).
		stringIsolate( queryStr, "_method=", "&", false, methodStr );
		Method method = Method::GET;
		if ( methodStr == "HEAD" ) {
			method = Method::HEAD;
		} else if ( methodStr == "POST" ) {
			method = Method::POST;
		} else if ( methodStr == "PUT" ) {
			method = Method::PUT;
		} else if ( methodStr == "PATCH" ) {
			method = Method::PATCH;
		} else if ( methodStr == "DELETE" ) {
			method = Method::DELETE;
		} else if ( methodStr == "OPTIONS" ) {
			method = Method::OPTIONS;
		}

		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		
		auto resource = this->m_resources.find( uriStr );
		if ( resource != this->m_resources.end() ) {

			std::stringstream headers;
			headers << "Content-Type: Content-type: application/json\r\n";
			headers << "Access-Control-Allow-Origin: *\r\n";

			auto cacheHit = this->m_resourceCache.find( uriStr );
			if ( cacheHit != this->m_resourceCache.end() ) {
				headers << "ETag: " << cacheHit->second.etag;
				
				// There's a cache entry available for the requested resource. If the client provided an
				// etag header that matches the one in the cache, nothing is being send. Otherwise the
				// cache is send.
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
				if ( etag == cacheHit->second.etag ) {
					mg_send_head( connection_, 304, 0, headers.str().c_str() );
				} else {
					mg_send_head( connection_, 200, cacheHit->second.content.length(), headers.str().c_str() );
					mg_send( connection_, cacheHit->second.content.c_str(), cacheHit->second.content.length() );
				}
			} else {

				// There's no cache entry available so we need to invoke all callbacks to generate the
				// content for this resource and populate the cache.
				json data;
				int code = 200;
				for ( auto callbackIt = resource->second.begin(); callbackIt != resource->second.end(); callbackIt++ ) {
					(*callbackIt)->callback( uriStr, code, data );
				}
				
				const std::string content = data.dump( 4 );
				const std::string etag = "W/\"" + std::to_string( rand() ) + "\"";
				const std::string modified = g_database->getQueryValue( "SELECT strftime('%%Y-%%m-%%d %%H:%%M:%%S GMT')" );
				this->m_resourceCache.insert( { uriStr, WebServer::ResourceCache( { content, etag, modified } ) } );

				headers << "ETag: " << etag << "\r\n";
				headers << "Last-Modified: " << modified;
				
				mg_send_head( connection_, 200, content.length(), headers.str().c_str() );
				mg_send( connection_, content.c_str(), content.length() );
			}
			
			connection_->flags |= MG_F_SEND_AND_CLOSE;

#ifdef _DEBUG
		} else if (
			uriStr == "api"
			&& method == Method::GET
		) {

			// Build a standard documentation page if the api uri is requested directly using get.
			std::stringstream outputStream;
			outputStream << "<!doctype html>" << "<html><body style=\"font-family:sans-serif;\">";
			for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
				outputStream << "<strong>Title:</strong> " << (*resourceIt->second.begin())->title << "<br>";
				outputStream << "<strong>Uri:</strong> <a href=\"/" << resourceIt->first << "\">" << resourceIt->first << "</a><br>";
				outputStream << "<strong>Methods:</strong>";
				
				unsigned int methods = 0;
				for ( auto callbackIt = resourceIt->second.begin(); callbackIt != resourceIt->second.end(); callbackIt++ ) {
					methods |= (*callbackIt)->methods;
				}
				std::map<Method,std::string> availableMethods = {
					{ Method::GET, "GET" },
					{ Method::HEAD, "HEAD" },
					{ Method::POST, "POST" },
					{ Method::PUT, "PUT" },
					{ Method::PATCH, "PATCH" },
					{ Method::DELETE, "DELETE" },
					{ Method::OPTIONS, "OPTIONS" },
				};
				for ( auto methodIt = availableMethods.begin(); methodIt != availableMethods.end(); methodIt++ ) {
					if ( ( methods & methodIt->first ) == methodIt->first ) {
						outputStream << " " << methodIt->second;
					}
				}

				outputStream << "<br><br>";
			}
			
			std::string output = outputStream.str();
			mg_send_head( connection_, 200, output.length(), "Content-Type: text/html" );
			mg_send( connection_, output.c_str(), output.length() );
			connection_->flags |= MG_F_SEND_AND_CLOSE;
#endif // _DEBUG

		} else {
			
			// If some other resource was accessed, serve static files instead.
			// TODO caching implementation
			// TODO gzip too?
			// TODO better 404 handling?
			mg_serve_http_opts options;
			memset( &options, 0, sizeof( options ) );
			options.document_root = "www";
			options.index_files = "index.html";
			options.enable_directory_listing = "no";
			mg_serve_http( connection_, message_, options );
		}
	}

}; // namespace micasa
