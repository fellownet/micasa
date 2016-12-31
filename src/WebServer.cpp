// restful
// http://www.vinaysahni.com/best-practices-for-a-pragmatic-restful-api
// http://www.restapitutorial.com/httpstatuscodes.html

// mongoose
// https://docs.cesanta.com/mongoose/master/
// https://github.com/Gregwar/mongoose-cpp/blob/master/mongoose/Server.cpp
// http://stackoverflow.com/questions/7698488/turn-a-simple-socket-into-an-ssl-socket
// https://github.com/cesanta/mongoose/blob/master/examples/simplest_web_server/simplest_web_server.c

// json
// https://github.com/nlohmann/json

#include <cstdlib>
#include <regex>

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
		
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void WebServer::stop() {
#ifdef _DEBUG
		assert( g_network->isRunning() && "Network should be running before WebServer is stopped." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};
	
	const std::chrono::milliseconds WebServer::_work( const unsigned long int& iteration_ ) {
		// TODO if it turns out that the webserver instance doesn't need to do stuff periodically, remove the
		// extend from Worker.
		return std::chrono::milliseconds( 1000 * 60 );
	};

	void WebServer::addResourceCallback( std::shared_ptr<ResourceCallback> callback_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		auto resourceIt = this->m_resources.find( callback_->uri );
		if ( resourceIt != this->m_resources.end() ) {
			resourceIt->second.push_back( callback_ );
		} else {
			this->m_resources[callback_->uri] = { callback_ };
		}
		this->_removeResourceCache( callback_->uri );
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
					this->_removeResourceCache( (*callbackIt)->uri );
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

	void WebServer::removeResourceCallbacksAt( const std::string uri_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		auto search = this->m_resources.find( uri_ );
		if ( search != this->m_resources.end() ) {
#ifdef _DEBUG
			g_logger->logr( Logger::LogLevel::DEBUG, this, "Resource callbacks removed at %s.", uri_.c_str() );
#endif // _DEBUG
			this->m_resources.erase( search );
			this->_removeResourceCache( uri_ );
		}
	};
	
	void WebServer::touchResourceCallback( const std::string reference_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); ) {
			for ( auto callbackIt = resourceIt->second.begin(); callbackIt != resourceIt->second.end(); ) {
				if ( (*callbackIt)->reference == reference_ ) {
					this->_removeResourceCache( (*callbackIt)->uri );
				}
				callbackIt++;
			}
			resourceIt++;
		}
	}
	
	void WebServer::touchResourceCallbacksAt( const std::string uri_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->_removeResourceCache( uri_ );
	};

	void WebServer::_removeResourceCache( const std::string uri_ ) {
		for ( auto cacheIt = this->m_resourceCache.begin(); cacheIt != this->m_resourceCache.end(); ) {
			if ( stringStartsWith( cacheIt->first, uri_ ) ) {
				cacheIt = this->m_resourceCache.erase( cacheIt );
			} else {
				cacheIt++;
			}
		}
	};
	
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

		// Create the input map which should consist of the parsed body and any parameter passed in the
		// query string.
		// TODO check for non /api requests
		json input;
		if ( ( method & ( Method::POST | Method::PUT | Method::PATCH ) ) > 0 ) {
			std::string bodyStr = "";
			bodyStr.assign( message_->body.p, message_->body.len );
			if ( bodyStr.size() > 0 ) {
				try {
					input = json::parse( bodyStr );
				} catch( ... ) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Invalid input json." );
				}
			}
		}
		
		// Parse the qyery string and put it's content into the input object.
		// TODO check for non /api requests
		std::regex pattern( "([\\w+%]+)=([^&]*)" );
		auto wordsBegin = std::sregex_iterator( queryStr.begin(), queryStr.end(), pattern );
		auto wordsEnd = std::sregex_iterator();
		for ( std::sregex_iterator it = wordsBegin; it != wordsEnd; it++ ) {
			std::string key = (*it)[1].str();
			std::string value = stringUriDecode( (*it)[2].str() );
			input[key] = value;
		}

		
		// Create a cacheKey for GET requests. Note that a std::map is sorted by default, hence the order
		// of parameters doesn't bypass the cache. Also, some parameters should not also not cause the
		// cache to be bypassed.
		// TODO check for non /api requests
		std::stringstream cacheKey;
		if ( method == Method::GET ) {
			cacheKey << uriStr;
			for ( auto inputIt = input.begin(); inputIt != input.end(); inputIt++ ) {
				if (
					inputIt.key() != "page"
					&& inputIt.key() != "count"
				) {
					cacheKey << "/" << inputIt.key() << "=" << inputIt.value();
				}
			}
		}

		std::unique_lock<std::mutex> resourceLock( this->m_resourcesMutex );
		
		auto resource = this->m_resources.find( uriStr );
		if ( resource != this->m_resources.end() ) {

			std::stringstream headers;
			headers << "Content-Type: Content-type: application/json\r\n";
			headers << "Access-Control-Allow-Origin: *\r\n";

			// TODO combine the cache with the query string (this already fails). Take care of pagination?
			std::map<std::string, ResourceCache>::iterator cacheHit;
			if (
				method == Method::GET
				&& ( cacheHit = this->m_resourceCache.find( cacheKey.str() ) ) != this->m_resourceCache.end()
			) {
				// Release the lock on the resource callbacks as soon as possible.
				resourceLock.unlock();
				
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
				// content for this resource and populate the cache (only if method == GET).
				json output;
				int code = 200;
				
				// Make a local copy of the callbacks to allow the lock to be released. This way callbacks can touch
				// resources which otherwise would cause a deadlock.
				auto callbacks = resource->second;
				resourceLock.unlock();

				// TODO take care of pagination here? can be efficient in combination with cache.
				
				// Call all callbacks configured for this uri.
				int called = 0;
				for ( auto callbackIt = callbacks.begin(); callbackIt != callbacks.end(); callbackIt++ ) {
					if ( ( (*callbackIt)->methods & method ) == method ) {
						(*callbackIt)->callback( uriStr, input, method, code, output );
						called++;
						if ( code != 200 ) {
							break;
						}
					}
				}

				// Generate output if no output was generated by the callbacks.
				if ( output.is_null() ) {
					if ( code == 200 ) {
						if ( called > 0 ) {
							output = {
								{ "result", "OK" }
							};
						} else {
							output = {
								{ "result", "ERROR" },
								{ "message", "Invalid resource." }
							};
						}
					} else {
						output = {
							{ "result", "ERROR" },
							{ "message", "Unknown error." }
						};
					}
				}
				
				// Format the json output (intent with 4 spaces).
				const std::string content = output.dump( 4 );
				
				// Only store the content in the cache if it's a get method. All other methods should not be cached.
				if ( method == Method::GET ) {
					const std::string etag = "W/\"" + std::to_string( rand() ) + "\"";
					const std::string modified = g_database->getQueryValue<std::string>( "SELECT strftime('%%Y-%%m-%%d %%H:%%M:%%S GMT')" );
					this->m_resourceCache.insert( { cacheKey.str(), WebServer::ResourceCache( { content, etag, modified } ) } );

					headers << "ETag: " << etag << "\r\n";
					headers << "Last-Modified: " << modified;
				} else {
					headers << "Cache-Control: no-cache, no-store, must-revalidate";
				}
				
				mg_send_head( connection_, code, content.length(), headers.str().c_str() );
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
				outputStream << "<strong>Uri:</strong> " << resourceIt->first << "<br>";
				outputStream << "<strong>Methods:</strong>";
				
				unsigned int methods = 0;
				for ( auto callbackIt = resourceIt->second.begin(); callbackIt != resourceIt->second.end(); callbackIt++ ) {
					methods |= (*callbackIt)->methods;
				}
				std::map<Method,std::string> availableMethods = {
					{ Method::GET, "<a href=\"" + resourceIt->first + "\">GET</a>" },
					{ Method::HEAD, "HEAD" },
					{ Method::POST, "POST" },
					{ Method::PUT, "<a href=\"" + resourceIt->first + "?_method=PUT\">PUT</a>" },
					{ Method::PATCH, "<a href=\"" + resourceIt->first + "?_method=PATCH\">PATCH</a>" },
					{ Method::DELETE, "<a href=\"" + resourceIt->first + "?_method=DELETE\">DELETE</a>" },
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

		} else if ( uriStr.substr( 0, 3 ) == "api" ) {

			std::stringstream headers;
			headers << "Content-Type: Content-type: application/json\r\n";
			headers << "Access-Control-Allow-Origin: *\r\n";
			headers << "Cache-Control: no-cache, no-store, must-revalidate";

			json output = {
				{ "result", "ERROR" },
				{ "message", "Invalid resource." }
			};
			int code = 404;
			
			// Format the json output (intent with 4 spaces).
			const std::string content = output.dump( 4 );
			
			mg_send_head( connection_, code, content.length(), headers.str().c_str() );
			mg_send( connection_, content.c_str(), content.length() );
			
			connection_->flags |= MG_F_SEND_AND_CLOSE;
			
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
