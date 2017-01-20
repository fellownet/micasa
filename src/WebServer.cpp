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
#include "Settings.h"
#include "User.h"

#include "json.hpp"

#define WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES 65

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Network> g_network;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Settings<> > g_settings;
	
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

		// RESTful is supposed to be stateless :) so the acls of the user are encrypted and send to the client.
		// We need a public/private key pair for that.
		if (
			g_settings->get( "public_key", "" ).empty()
			|| g_settings->get( "private_key", "" ).empty()
		) {
			std::string publicKey;
			std::string privateKey;
			if ( generateKeys( publicKey, privateKey ) ) {
				this->m_publicKey = publicKey;
				this->m_privateKey = privateKey;

				g_settings->put( "public_key", publicKey );
				g_settings->put( "private_key", privateKey );
				g_settings->commit();

				g_logger->log( Logger::LogLevel::NORMAL, this, "New ssl keys generated." );
			} else {
				g_logger->log( Logger::LogLevel::ERROR, this, "Error while generating ssl keys." );
			}
		}

		// If there are no users defined in the database, a default administrator is created.
		if ( g_database->getQueryValue<unsigned int>( "SELECT COUNT(*) FROM `users`" ) == 0 ) {
			g_database->putQuery(
				"INSERT INTO `users` (`name`, `username`, `password`, `rights`) "
				"VALUES ( 'Administrator', 'admin', '%s', %d )",
				generateHash( "admin", this->m_privateKey ).c_str(),
				User::resolveRights( User::Rights::ADMIN )
			);
			g_logger->log( Logger::LogLevel::NORMAL, this, "Default administrator user created." );
		}

		this->_installUserResourceHandler();
		
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void WebServer::stop() {
#ifdef _DEBUG
		assert( g_network->isRunning() && "Network should be running before WebServer is stopped." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		this->removeResourceCallback( "webserver-users" );

		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};
	
	void WebServer::addResourceCallback( const ResourceCallback& callback_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		auto resourceIt = this->m_resources.find( callback_.uri );
		if ( resourceIt != this->m_resources.end() ) {
			resourceIt->second.push_back( std::make_shared<WebServer::ResourceCallback>( callback_ ) );
			std::sort( resourceIt->second.begin(), resourceIt->second.end(), [=]( std::shared_ptr<ResourceCallback>& a_, std::shared_ptr<ResourceCallback>& b_ ) {
				return a_->sort < b_->sort;
			} );
		} else {
			this->m_resources[callback_.uri] = { std::make_shared<WebServer::ResourceCallback>( callback_ ) };
		}
#ifdef _DEBUG
		g_logger->logr( Logger::LogLevel::DEBUG, this, "Resource callback installed at %s.", callback_.uri.c_str() );
#endif // _DEBUG
	};

	void WebServer::removeResourceCallback( const std::string& reference_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); ) {
			for ( auto callbackIt = resourceIt->second.begin(); callbackIt != resourceIt->second.end(); ) {
				if ( (*callbackIt)->reference == reference_ ) {
#ifdef _DEBUG
					g_logger->logr( Logger::LogLevel::DEBUG, this, "Resource callback removed at %s.", (*callbackIt)->uri.c_str() );
#endif // _DEBUG
					callbackIt = resourceIt->second.erase( callbackIt );
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

	void WebServer::_processHttpRequest( mg_connection* connection_, http_message* message_ ) {

		// Determine wether or not the request is an API request or a request for static files. Static files
		// are handled by Mongoose internally.
		std::string uri;
		if ( message_->uri.len >= 1 ) {
			uri.assign( message_->uri.p + 1, message_->uri.len - 1 );
		}
		if ( uri.substr( 0, 3 ) != "api" ) {

			// This is NOT an API request, serve static files instead.
			mg_serve_http_opts options;
			memset( &options, 0, sizeof( options ) );
			options.document_root = "www";
			options.index_files = "index.html";
			options.enable_directory_listing = "no";
			mg_serve_http( connection_, message_, options );

#ifdef _DEBUG

		} else if ( uri == "api" ) {

			// Serve the list of API endpoints (resource callbacks) at /api if we're running a DEBUG build.
			std::stringstream outputStream;
			outputStream << "<!doctype html>" << "<html><body style=\"font-family:sans-serif;\">";
			for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
				outputStream << "<strong>Uri:</strong> " << resourceIt->first << "<br>";
				outputStream << "<strong>Methods:</strong>";
				
				Method methods = WebServer::resolveMethod( 0 );
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

			// Extract headers from http message.
			std::map<std::string, std::string> headers;
			int i = 0;
			for ( const mg_str& name : message_->header_names ) {
				std::string header, value;
				header.assign( name.p, name.len );
				if ( ! header.empty() ) {
					value.assign( message_->header_values[i].p, message_->header_values[i].len );
					headers[header] = value;
				}
				i++;
			}

			// Parse query string. If we encounter a method override (using _method=POST for instance) it is
			// skipped and used as methodStr. If we encounter a token override (using _token=xxx) it is used
			// as an Authorization header.
			std::string methodStr;
			methodStr.assign( message_->method.p, message_->method.len );

			std::string query;
			query.assign( message_->query_string.p, message_->query_string.len );

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

				if ( key == "_method" ) {
					methodStr = value;
				} else if ( key == "_token" ) {
					headers["Authorization"] = value;
				} else {
					params[key] = value;
				}
			}
			
			// Determine method (note that the method can be overridden by adding _method=xx to the query).
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

			// Prepare the input json object that holds all the supplied parameters (both in the body as in
			// the query string).
			json input = json::object();
			if (
				WebServer::resolveMethod( method & ( Method::POST | Method::PUT | Method::PATCH ) ) > 0
				&& message_->body.len > 2
			) {
				std::string body;
				body.assign( message_->body.p, message_->body.len );
				try { input = json::parse( body ); } catch( ... ) { }
			}
			if ( ! input.is_object() ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Invalid json in request body." );
				input = json::object();
			}
			for ( auto paramsIt = params.begin(); paramsIt != params.end(); paramsIt++ ) {
				input[(*paramsIt).first] = (*paramsIt).second;
			}



			// Determine the access rights of the current client. If a valid token was provided, details of
			// the token are added to the input aswell.
			User::Rights rights = User::Rights::PUBLIC;
			if ( headers.find( "Authorization" ) != headers.end() ) {
				try {
					json token = json::parse( decrypt( headers["Authorization"], this->m_publicKey ) );
					rights = User::resolveRights( token["rights"].get<int>() );
					input["authorization"] = token;
					
					// See if there's a matching user in the database with the same rights.
//					auto result = g_database->getQueryRow(
//						"SELECT u.`rights`, strftime('%%s') AS now "
//						"FROM `users` u "
//						"WHERE `id`=%d ",
//						token["user_id"].get<unsigned int>()
//					);
//					if (
//						token["rights"].get<int>() == std::stoi( result["rights"] )
//						&& token["valid"].get<int>() >= std::stoi( result["now"] )
//					) {
//						rights = User::resolveRights( token["rights"].get<int>() );
//					}
//					input["authorization"] = token;
				} catch( ... ) { }
			}

			// Prepare the output json object that is eventually send back to the client. Each API request
			// results in a standardized response.
			json output = {
				{ "result", "OK" },
				{ "code", 204 }, // no content
			};

			// Find the requested resource in a critical section.
			std::unique_lock<std::mutex> resourceLock( this->m_resourcesMutex );
			auto resource = this->m_resources.begin();
			for ( resource = this->m_resources.begin(); resource != this->m_resources.end(); resource++ ) {
				std::regex pattern( resource->first );
				std::smatch match;
				if ( std::regex_search( uri, match, pattern ) ) {
					
					// Add each matched paramter to the input for the callback to determine which individual
					// resource was accessed.
					for ( unsigned int i = 1; i < match.size(); i++ ) {
						if ( match.str( i ).size() > 0 ) {
							input["$" + std::to_string( i )] = match.str( i );
						}
					}
					
					break;
				}
			}
			if ( resource != this->m_resources.end() ) {
			
				// The entire vector of callbacks is copied to a local variable to be able to call each
				// callback with the resource lock released. Note that the vector contains shared pointers
				// which will increase their ref count and not delete the callback until this scope ends,
				// even then the callback is removed while iterating.
				auto callbacks = (*resource).second;
				resourceLock.unlock();

				try {
					unsigned int noAccess = 0;
					for ( auto callbackIt = callbacks.begin(); callbackIt != callbacks.end(); callbackIt++ ) {
						if ( ( (*callbackIt)->methods & method ) == method ) {
							if ( rights >= (*callbackIt)->rights ) {
								(*callbackIt)->callback( input, method, output );
							} else {
								noAccess++;
							}
						}
					}
					if (
						noAccess > 0
						&& output["code"].get<unsigned int>() != 200
					) {
						output["result"] = "ERROR";
						output["code"] = 401;
						output["error"] = "Resource.Access.Denied";
						output["message"] = "Access denied to requested resource.";
					}
				} catch( const ResourceException& exception_ ) {
					output["result"] = "ERROR";
					output["code"] = exception_.code;
					output["error"] = exception_.error;
					output["message"] = exception_.message;
//				} catch( ... ) {
//					output["result"] = "ERROR";
//					output["code"] = 500;
//					output["error"] = "Resource.Failure";
//					output["message"] = "The requested resource failed to load.";
				}

			} else {
				resourceLock.unlock();

				output["result"] = "ERROR";
				output["code"] = 404;
				output["error"] = "Resource.Not.Found";
				output["message"] = "The requested resource was not found.";
			}

#ifdef _DEBUG
			assert( output.find( "result" ) != output.end() && output["result"].is_string() && "API requests should contain a string result property." );
			assert( output.find( "code" ) != output.end() && output["code"].is_number() && "API requests should contain a numeric code property." );
			const std::string content = output.dump( 4 );
#else
			const std::string content = output.dump();
#endif // _DEBUG

			unsigned int code = output["code"].get<unsigned int>();

			std::stringstream headersOut;
			headersOut << "Content-Type: Content-type: application/json\r\n";
			headersOut << "Access-Control-Allow-Origin: *\r\n";
			headersOut << "Cache-Control: no-cache, no-store, must-revalidate";

			mg_send_head( connection_, code, content.length(), headersOut.str().c_str() );
			if ( code != 304 ) { // Not Modified
				mg_send( connection_, content.c_str(), content.length() );
			}
			
			connection_->flags |= MG_F_SEND_AND_CLOSE;
		}
	};

	void WebServer::_installUserResourceHandler() {
		this->addResourceCallback( {
			"webserver-users",
			"^api/user/(login|refresh)$",
			100,
			User::Rights::PUBLIC,
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const json& input_, const WebServer::Method& method_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_usersMutex );

				json user;
				
				// A client is allowed to provide a requested duration for the token.
				unsigned int duration = WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES;
				auto find = input_.find( "duration");
				if ( find != input_.end() ) {
					if ( (*find).is_number() ) {
						duration = (*find).get<unsigned int>();
					} else {
						throw WebServer::ResourceException( { 400, "Login.Invalid.Duration", "The supplied duration is invalid." } );
					}
				}
				
				if (
					method_ == WebServer::Method::POST
					&& input_["$1"].get<std::string>() == "login"
				) {
					std::string username;
					std::string password;
					
					auto find = input_.find( "username");
					if ( find == input_.end() ) {
						throw WebServer::ResourceException( { 400, "Login.Missing.Username", "Missing username." } );
					} else if ( ! (*find).is_string() ) {
						throw WebServer::ResourceException( { 400, "Login.Invalid.Username", "The supplied username is invalid." } );
					} else {
						username = (*find).get<std::string>();
					}
					
					find = input_.find( "password");
					if ( find == input_.end() ) {
						throw WebServer::ResourceException( { 400, "Login.Missing.Password", "Missing password." } );
					} else if ( ! (*find).is_string() ) {
						throw WebServer::ResourceException( { 400, "Login.Invalid.Password", "The supplied password is invalid." } );
					} else {
						password = generateHash( (*find).get<std::string>(), this->m_privateKey );
					}
					
					try {
						user = g_database->getQueryRow<json>(
							"SELECT `id`, `name`, `username`, `rights`, `enabled`, CAST(strftime('%%s','now','+%d minute') AS INTEGER) AS `valid` "
							"FROM `users` "
							"WHERE `username`='%s' "
							"AND `password`='%s' "
							"AND `enabled`=1",
							duration,
							username.c_str(),
							password.c_str()
						);
					} catch( const Database::NoResultsException& ex_ ) {
						throw WebServer::ResourceException( { 400, "Login.Failure", "The username and/or password is invalid." } );
					}
					output_["code"] = 201; // Created
				} else if (
					method_ == WebServer::Method::GET
					&& input_["$1"].get<std::string>() == "refresh"
				) {
					auto find = input_.find( "authorization" );
					if (
						find == input_.end()
						|| ! (*find).is_object()
					) {
						throw WebServer::ResourceException( { 401, "Resource.Access.Denied", "Access denied to requested resource." } );
					}
					user = g_database->getQueryRow<json>(
						"SELECT `id`, `name`, `username`, `rights`, `enabled`, CAST(strftime('%%s','now','+%d minute') AS INTEGER) AS `valid` "
						"FROM `users` "
						"WHERE `id`=%d "
						"AND `enabled`=1",
						duration,
						input_["authorization"]["user_id"].get<unsigned int>()
					);
					output_["code"] = 200;
				} else {
					throw WebServer::ResourceException( { 404, "Resource.Not.Found", "The requested resource was not found." } );
				}

				json token = {
					{ "user_id", user["id"].get<unsigned int>() },
					{ "rights", user["rights"].get<unsigned int>() },
					{ "valid", user["valid"].get<unsigned int>() }
				};
				auto valid = user["valid"].get<unsigned int>();
				user.erase( "valid" );
				output_["data"] = {
					{ "user", user },
					{ "valid", valid },
					{ "token", encrypt( token.dump(), this->m_privateKey ) }
				};
			} )
		} );

		this->addResourceCallback( {
			"webserver-users",
			"^api/users(/([0-9]+))?$",
			100,
			User::Rights::ADMIN,
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const json& input_, const WebServer::Method& method_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_usersMutex );
			
				json user = json::object();
				int userId = -1;
				auto find = input_.find( "$2" ); // inner uri regexp match
				if ( find != input_.end() ) {
					try {
						userId = std::stoi( (*find).get<std::string>() );
						user = g_database->getQueryRow<json>(
							"SELECT `id`, `name`, `username`, `rights`, `enabled` "
							"FROM `users` "
							"WHERE `id`=%d ",
							userId
						);
					} catch( ... ) {
						throw WebServer::ResourceException( { 404, "Resource.Not.Found", "The requested resource was not found." } );
					}
				}

				switch( method_ ) {

					case WebServer::Method::GET: {
						if ( userId != -1 ) {
							output_["data"] = user;
						} else {
							output_["data"] = g_database->getQuery<json>(
								"SELECT `id`, `name`, `username`, `rights`, `enabled` "
								"FROM `users` "
								"ORDER BY `id` ASC"
							);
						}
						output_["code"] = 200;
						break;
					}
					
					case WebServer::Method::DELETE: {
						if ( userId != -1 ) {
							g_database->putQuery(
								"DELETE FROM `users` "
								"WHERE `id`=%d",
								userId
							);
							std::string setting = WEBSERVER_SETTING_USER_SETTINGS_PREFIX + std::to_string( userId );
							g_settings->remove( setting );
							g_settings->commit();
							output_["code"] = 200;
						} else {
							throw WebServer::ResourceException( { 404, "Resource.Not.Found", "The requested resource was not found." } );
						}
						break;
					}
					
					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						auto find = input_.find( "name" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								user["name"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( { 400, "User.Invalid.Name", "The supplied name is invalid." } );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( { 400, "User.Missing.Name", "Missing name." } );
						}

						find = input_.find( "username" );
						if ( find != input_.end() ) {
							if (
								(*find).is_string()
								&& (*find).get<std::string>().size() > 2
								&& (*find).get<std::string>().size() <= 32
							) {
								user["username"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( { 400, "User.Invalid.Username", "The supplied username is invalid." } );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( { 400, "User.Missing.Username", "Missing username." } );
						}

						find = input_.find( "password" );
						if ( find != input_.end() ) {
							if (
								(*find).is_string()
								&& (*find).get<std::string>().size() > 2
								&& (*find).get<std::string>().size() <= 32
							) {
								user["password"] = generateHash( (*find).get<std::string>(), this->m_privateKey );
							} else {
								throw WebServer::ResourceException( { 400, "User.Invalid.Password", "The supplied password is invalid." } );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( { 400, "User.Missing.Password", "Missing password." } );
						}
						
						find = input_.find( "rights" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								user["rights"] = std::stoi( (*find).get<std::string>() );
							} else if ( (*find).is_number() ) {
								user["rights"] = (*find).get<unsigned short>();
							} else {
								throw WebServer::ResourceException( { 400, "User.Invalid.Rights", "The supplied rights are invalid." } );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( { 400, "User.Missing.Rights", "Missing rights." } );
						}

						find = input_.find( "enabled" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								user["enabled"] = ( (*find).get<std::string>() == "1" || (*find).get<std::string>() == "true" || (*find).get<std::string>() == "yes" );
							} else if ( (*find).is_number() ) {
								user["enabled"] = (*find).get<unsigned short>() > 0;
							} else if ( (*find).is_boolean() ) {
								user["enabled"] = (*find).get<bool>();
							} else {
								throw WebServer::ResourceException( { 400, "User.Invalid.Enabled", "The supplied enabled parameter is invalid." } );
							}
						} else if ( userId == -1 ) {
							user["enabled"] = true;
						}
						
						if ( userId == -1 ) {
							userId = g_database->putQuery(
								"INSERT INTO `users` (`name`, `username`, `password`, `rights`, `enabled`) "
								"VALUES (%Q, %Q, %Q, %d, %d) ",
								user["name"].get<std::string>().c_str(),
								user["username"].get<std::string>().c_str(),
								user["password"].get<std::string>().c_str(),
								user["rights"].get<unsigned int>(),
								user["enabled"].get<bool>() ? 1 : 0
							);
							user["id"] = userId;
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `users` "
								"SET `name`=%Q, `username`=%Q, `password`=%Q, `rights`=%d, `enabled`=%d "
								"WHERE `id`=%d",
								user["name"].get<std::string>().c_str(),
								user["username"].get<std::string>().c_str(),
								user["password"].get<std::string>().c_str(),
								user["rights"].get<unsigned int>(),
								user["enabled"].get<bool>() ? 1 : 0,
								userId
							);
							output_["code"] = 200;
						}
						output_["data"] = user;
						break;
					}

					default: break;
				}
			} )
		} );
		
		this->addResourceCallback( {
			"webserver-users",
			"^api/user/settings$",
			100,
			User::Rights::VIEWER,
			WebServer::Method::GET | WebServer::Method::PUT,
			WebServer::t_callback( [this]( const json& input_, const WebServer::Method& method_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_usersMutex );

				auto find = input_.find( "authorization" );
				if (
					find == input_.end()
					|| ! (*find).is_object()
				) {
					throw WebServer::ResourceException( { 401, "Resource.Access.Denied", "Access denied to requested resource." } );
				}
				
				std::string setting = WEBSERVER_SETTING_USER_SETTINGS_PREFIX + std::to_string( (*find)["user_id"].get<unsigned int>() );
				
				switch( method_ ) {

					case WebServer::Method::GET: {
						output_["data"] = json::parse( g_settings->get( setting, "{}" ) );
						break;
					}
					
					case WebServer::Method::PUT: {
						find = input_.find( "settings" );
						if ( find != input_.end() ) {
							if ( (*find).is_object() ) {
								g_settings->put( setting, (*find).dump() );
								g_settings->commit();
							} else {
								throw WebServer::ResourceException( { 400, "User.Invalid.Settings", "The supplied settings are invalid." } );
							}
						} else {
							throw WebServer::ResourceException( { 400, "User.Missing.Settings", "Missing settings." } );
						}
						break;
					}
					default: break;
				}
			
				output_["code"] = 200;
			} )
		} );
	};

}; // namespace micasa
