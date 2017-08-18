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
#include <sstream>

#ifdef _WITH_OPENSSL
	#include <openssl/x509.h>
	#include <openssl/pem.h>
#endif

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "WebServer.h"

#include "Controller.h"
#include "Database.h"
#include "Logger.h"
#include "Settings.h"
#include "User.h"

#ifdef _DEBUG
	#include "plugins/Debug.h"
#endif // _DEBUG
#include "plugins/Dummy.h"
#include "plugins/HarmonyHub.h"
#ifdef _WITH_LINUX_SPI
	#include "plugins/PiFace.h"
#endif // _WITH_LINUX_SPI
#include "plugins/RFXCom.h"
#include "plugins/SolarEdge.h"
#include "plugins/Telegram.h"
#include "plugins/WeatherUnderground.h"
#ifdef _WITH_OPENZWAVE
	#include "plugins/ZWave.h"
#endif // _WITH_OPENZWAVE

#include "json.hpp"

#define WEBSERVER_HASH_MAGGI "6238fbba-1f79-11e7-93ae-92361f002671"

namespace micasa {

	extern std::unique_ptr<Database> g_database;
	extern std::unique_ptr<Controller> g_controller;
	extern std::unique_ptr<Settings<>> g_settings;

	using namespace std::chrono;
	using namespace nlohmann;

	const std::map<WebServer::Method, std::string> WebServer::MethodText = {
		{ WebServer::Method::GET, "GET" },
		{ WebServer::Method::HEAD, "HEAD" },
		{ WebServer::Method::POST, "POST" },
		{ WebServer::Method::PUT, "PUT" },
		{ WebServer::Method::PATCH, "PATCH" },
		{ WebServer::Method::DELETE, "DELETE" },
		{ WebServer::Method::OPTIONS, "OPTIONS" }
	};

	WebServer::WebServer( unsigned int port_, unsigned int sslport_ ) :
		m_port( port_ ),
		m_sslport( sslport_ ),
		m_resources( std::vector<t_resource>( 9 ) )
	{
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before global WebServer instance." );
		assert( g_controller && "Global Controller instance should be created before global WebServer instance." );
#endif // _DEBUG
		srand ( time( NULL ) );
	};

	WebServer::~WebServer() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after global WebServer instance." );
		assert( g_controller && "Global Controller instance should be destroyed after global WebServer instance." );
#endif // _DEBUG
	};

	void WebServer::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		// Micasa only runs over HTTPS which requires a keys and certificate file. These are read and/or stored in the
		// userdata folder. Webtokens / authentication is also encrypted using the same keys.
		// http://stackoverflow.com/questions/256405/programmatically-create-x509-certificate-using-openssl

#ifdef _WITH_OPENSSL
		if ( this->m_sslport > 0 ) {
			if (
				access( ( std::string( _DATADIR ) + "/key.pem" ).c_str(), F_OK ) != 0
				|| access( ( std::string( _DATADIR ) + "/cert.pem" ).c_str(), F_OK ) != 0
			) {
				Logger::log( Logger::LogLevel::NORMAL, this, "Generating SSL key and certificate." );

				EVP_PKEY* key = EVP_PKEY_new();
				RSA* rsa = RSA_generate_key(
					2048,   // number of bits for the key
					RSA_F4, // exponent
					NULL,   // callback - can be NULL if we aren't displaying progress
					NULL    // callback argument - not needed in this case
				);
				if ( ! rsa ) {
					throw std::runtime_error( "unable to generate 2048-bit RSA key" );
				}
				EVP_PKEY_assign_RSA( key, rsa ); // now rsa will be free'd alongside m_key

				X509* certificate = X509_new();
				ASN1_INTEGER_set( X509_get_serialNumber( certificate ), 1 ); // some webservers reject serial number 0
				X509_gmtime_adj( X509_get_notBefore( certificate ), 0 ); // not usable before 'now'
				X509_gmtime_adj( X509_get_notAfter( certificate ), (long)( 60 * 60 * 24 * 365 ) ); // not usable after xx days
				X509_set_pubkey( certificate, key );

				X509_NAME* name = X509_get_subject_name( certificate );
				X509_NAME_add_entry_by_txt( name, "C", MBSTRING_ASC, (unsigned char*)"NL", -1, -1, 0 );
				X509_NAME_add_entry_by_txt( name, "CN", MBSTRING_ASC, (unsigned char*)"Micasa", -1, -1, 0 );
				X509_NAME_add_entry_by_txt( name, "O", MBSTRING_ASC, (unsigned char*)"Fellownet", -1, -1, 0 );
				X509_set_issuer_name( certificate, name );

				// Self-sign the certificate with the private key (NOTE: when using real certificates, a CA should sign the
				// certificate). So the certificate contains the public key and is signed with the private key. This makes
				// sense for self-signed certificates.
				X509_sign( certificate, key, EVP_sha1() );

				// The private key and the certificate are written to disk in PEM format.
				FILE* f = fopen( ( std::string( _DATADIR ) + "/key.pem" ).c_str(), "wb");
				PEM_write_PrivateKey(
					f,            // write the key to the file we've opened
					key,          // our key from earlier
					NULL,         // default cipher for encrypting the key on disk (EVP_des_ede3_cbc())
					NULL,         // passphrase required for decrypting the key on disk ("replace_me")
					0,            // length of the passphrase string (10)
					NULL,         // callback for requesting a password
					NULL          // data to pass to the callback
				);
				fclose( f );

				f = fopen( ( std::string( _DATADIR ) + "/cert.pem" ).c_str(), "wb" );
				PEM_write_X509(
					f,            // write the certificate to the file we've opened
					certificate   // our certificate
				);
				fclose( f );
				X509_free( certificate );
				EVP_PKEY_free( key );
			}
		}
#endif

		this->_installPluginResourceHandler();
		this->_installDeviceResourceHandler();
		this->_installLinkResourceHandler();
		this->_installScriptResourceHandler();
		this->_installTimerResourceHandler();
		this->_installUserResourceHandler();

		auto handler = [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
			if ( event_ == Network::Connection::Event::HTTP ) {
				this->_processRequest( connection_ );
			}
		};
		if ( this->m_port > 0 ) {
			this->m_bind = Network::bind( std::to_string( this->m_port ), handler );
			if ( ! this->m_bind ) {
				Logger::logr( Logger::LogLevel::ERROR, this, "Unable to bind to port %d.", this->m_port );
			}
		}
#ifdef _WITH_OPENSSL
		if ( this->m_sslport > 0 ) {
			this->m_sslbind = Network::bind( std::to_string( this->m_sslport ), std::string( _DATADIR ) + "/cert.pem", std::string( _DATADIR ) + "/key.pem", handler );
			if ( ! this->m_sslbind ) {
				Logger::logr( Logger::LogLevel::ERROR, this, "Unable to bind to port %d.", this->m_sslport );
			}
		}
#endif // _WITH_OPENSSL

		// If there are no users defined in the database, a default administrator is created.
		if ( g_database->getQueryValue<unsigned int>( "SELECT COUNT(*) FROM `users`" ) == 0 ) {
			std::string salt = randomString( 64 );
			std::string pepper = g_settings->get( WEBSERVER_SETTING_HASH_PEPPER, randomString( 64 ) );
			g_settings->put( WEBSERVER_SETTING_HASH_PEPPER, pepper );
			g_settings->commit();
			std::string password = this->_hash( salt + pepper + "admin" + WEBSERVER_HASH_MAGGI ) + '.' + salt;
			g_database->putQuery(
				"INSERT INTO `users` (`name`, `username`, `password`, `rights`) "
				"VALUES ( 'Administrator', 'admin', '%s', %d )",
				password.c_str(),
				User::resolveRights( User::Rights::ADMIN )
			);
			Logger::log( Logger::LogLevel::NORMAL, this, "Default administrator user created." );
		}

		this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, SCHEDULER_INTERVAL_5MIN, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			std::lock_guard<std::mutex> lock( this->m_loginsMutex );
			auto now = system_clock::now();
			for ( auto loginIt = this->m_logins.begin(); loginIt != this->m_logins.end(); ) {
				if ( (*loginIt).second.valid < now ) {
					for ( auto connectionIt = loginIt->second.sockets.begin(); connectionIt != loginIt->second.sockets.end(); connectionIt++ ) {
						auto connection = (*connectionIt).lock();
						if ( connection ) {
							connection->close();
						}
					}
					loginIt = this->m_logins.erase( loginIt );
				} else {
					loginIt++;
				}
			}
		} );

		Logger::log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void WebServer::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		if ( this->m_bind ) {
			this->m_bind->terminate();
		}

		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );

		Logger::log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	void WebServer::broadcast( const std::string& message_ ) {
		this->m_scheduler.schedule( 0, 1, this, [=]( std::shared_ptr<Scheduler::Task<>> ) {
			std::lock_guard<std::mutex> lock( this->m_loginsMutex );
			for ( auto loginIt = this->m_logins.begin(); loginIt != this->m_logins.end(); loginIt++ ) {
				for ( auto connectionIt = loginIt->second.sockets.begin(); connectionIt != loginIt->second.sockets.end(); ) {
					auto connection = (*connectionIt).lock();
					if ( connection ) {
						connection->send( message_ );
						connectionIt++;
					} else {
						connectionIt = loginIt->second.sockets.erase( connectionIt );
					}
				}
			}
		} );
	};

	std::string WebServer::_hash( const std::string& data_ ) const {
#ifdef _WITH_OPENSSL
		SHA256_CTX context;
		if ( ! SHA256_Init( &context ) ) {
			throw std::runtime_error( "sha256 init failure" );
		}
		unsigned char hash[SHA256_DIGEST_LENGTH];
		if (
			! SHA256_Update( &context, data_.c_str(), data_.size() )
			|| ! SHA256_Final( hash, &context )
		) {
			throw std::runtime_error( "sha256 failure" );
		}
		std::stringstream ss;
		for ( int i = 0; i < SHA256_DIGEST_LENGTH; i++ ) {
			ss << std::hex << std::setw( 2 ) << std::setfill( '0' ) << (int)hash[i];
		}
		return ss.str();
#else
		// NOTE do not use in production :) don't ignore the cmake warning.
		return data_ + "#HASHED";
#endif
	};

	inline void WebServer::_processRequest( std::shared_ptr<Network::Connection> connection_ ) {
		auto uri = connection_->getUri();
		auto headers = connection_->getHeaders();

		// A websocket connection starts of as a regular http request with an additional header requesting to upgrade
		// the connection once the http handshake is done.
		auto find = headers.find( "Upgrade" );
		if ( __unlikely(
			find != headers.end()
			&& uri.substr( 0, 5 ) == "/live"
		) ) {
			std::string token = uri.substr( 6 );
			std::lock_guard<std::mutex> lock( this->m_loginsMutex );
			auto find = this->m_logins.find( token );
			if (
				find != this->m_logins.end()
				&& find->second.valid > system_clock::now()
			) {
				find->second.sockets.push_back( connection_ );
			}

		// Serve static files for requests NOT targetting the api.
		} else if ( __unlikely( uri.substr( 0, 4 ) != "/api" ) ) {

			connection_->serve( "www" );

		// Serve dynamic data for requests targettig the api.
		} else {

			auto method = WebServer::resolveTextMethod( connection_->getMethod() );
			auto data = connection_->getBody();
			auto params = connection_->getParams();

			// Some query paramters might override other request characteristics.
			try {
				method = WebServer::resolveTextMethod( params.at( "_method" ) );
			} catch( ... ) { };
			try {
				headers["Authorization"] = params.at( "_token" );
			} catch( ... ) { };

			this->m_scheduler.schedule( 0, 1, this, [this,connection_,uri,method,headers,data,params]( std::shared_ptr<Scheduler::Task<>> ) {

				// Prepare the input json object that holds all the supplied parameters (both in the body as in the query
				// string).
				json input = json::object();
				if (
					WebServer::resolveMethod( method & ( Method::POST | Method::PUT | Method::PATCH ) ) > 0
					&& data.length() > 2
				) {
					try {
						input = json::parse( data );
					} catch( json::exception ex_ ) {
						Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
					}
				}
				for ( auto paramsIt = params.begin(); paramsIt != params.end(); paramsIt++ ) {
					input[(*paramsIt).first] = (*paramsIt).second;
				}

				// If a username/password combination, or an authorization token was provided, match it with a user or
				// an existing login. The user is stored in the login table in the login/refresh resource.
				std::shared_ptr<User> user = nullptr;
				if (
					input.find( "username" ) != input.end()
					&& input.find( "password" ) != input.end()
				) {
					try {
						auto data = g_database->getQueryRow(
							"SELECT `id`, `name`, `rights`, `password` "
							"FROM `users` "
							"WHERE `username`='%s' "
							"AND `enabled`=1",
							input["username"].get<std::string>().c_str()
						);
						auto parts = stringSplit( data["password"], '.' );
						std::string pepper = g_settings->get( WEBSERVER_SETTING_HASH_PEPPER );
						if ( parts[0] == this->_hash( parts[1] + pepper + input["password"].get<std::string>() + WEBSERVER_HASH_MAGGI ) ) {
							user = std::make_shared<User>(
								std::stoi( data["id"] ),
								data["name"],
								User::resolveRights( std::stoi( data["rights"] ) )
							);
						}
					} catch( Database::NoResultsException ex_ ) {
						// Do not log this error (yet), a new user might be created.
					}
				}
				if (
					user == nullptr
					&& headers.find( "Authorization" ) != headers.end()
				) {
					try {
						std::unique_lock<std::mutex> loginsLock( this->m_loginsMutex );
						std::string token = headers.at( "Authorization" );
						auto login = this->m_logins.at( token );
						if ( login.valid > system_clock::now() ) {
							user = login.user;
							input["_token"] = token;
						}
						loginsLock.unlock();
					} catch( std::out_of_range ex_ ) {
						Logger::log( Logger::LogLevel::WARNING, this, "Invalid authorization token supplied." );
					}
				}

				// Prepare the output json object that is eventually send back to the client.
				json output = {
					{ "result", "OK" },
					{ "code", 204 }, // no content
				};

				try {
					for ( auto &resource : this->m_resources ) {
						std::regex pattern( resource.uri );
						std::smatch match;
						if (
							std::regex_search( uri, match, pattern )
							&& ( resource.methods & method ) == method
						) {
							// Add each matched paramter to the input for the callback to determine which individual
							// resource was accessed.
							for ( unsigned int i = 1; i < match.size(); i++ ) {
								if ( match.str( i ).size() > 0 ) {
									input["$" + std::to_string( i )] = match.str( i );
								}
							}
							resource.callback( user, input, method, output );
						}
					}

					// If no callbacks we're called the code should still be 204 and should be replaced with a 404
					// indicating a resource not found error.
					if ( output["code"].get<unsigned int>() == 204 ) {
						output["result"] = "ERROR";
						output["code"] = 404;
						output["error"] = "Resource.Not.Found";
						output["message"] = "The requested resource was not found.";
					}

				} catch( const ResourceException& exception_ ) {
					output["result"] = "ERROR";
					output["code"] = exception_.code;
					output["error"] = exception_.error;
					output["message"] = exception_.message;
				} catch( std::exception& exception_ ) { // also catches json::exception
					output["result"] = "ERROR";
					output["code"] = 500;
					output["error"] = "Resource.Failure";
					output["message"] = "The requested resource failed to load.";
					Logger::log( Logger::LogLevel::ERROR, this, exception_.what() );
#ifndef _DEBUG
				} catch( ... ) {
					output["result"] = "ERROR";
					output["code"] = 500;
					output["error"] = "Resource.Failure";
					output["message"] = "The requested resource failed to load.";
#endif // _DEBUG
				}

#ifdef _DEBUG
				assert( output.find( "result" ) != output.end() && output["result"].is_string() && "API requests should contain a string result property." );
				assert( output.find( "code" ) != output.end() && output["code"].is_number() && "API requests should contain a numeric code property." );
				const std::string content = output.dump( 4 );
#else
				const std::string content = output.dump();
#endif // _DEBUG

				unsigned int code = output["code"].get<unsigned int>();
				this->m_scheduler.schedule( code == 401 ? SCHEDULER_INTERVAL_3SEC : 0, 1, this, [connection_,content,code]( std::shared_ptr<Scheduler::Task<>> ) {
					connection_->reply( content, code, {
						{ "Content-Type", "Content-type: application/json" },
						{ "Access-Control-Allow-Origin", "*" },
						{ "Cache-Control", "no-cache, no-store, must-revalidate" }
					} );
				} );
			} );
		}
	};

	void WebServer::_installPluginResourceHandler() {
		this->m_resources[0] = {
			"^/api/plugins(/([0-9,]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::INSTALLER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				auto fGetSettings = []( std::shared_ptr<Plugin> plugin_ = nullptr ) {
					if ( __unlikely( plugin_ == nullptr ) ) {
						json settings = json::array();
						settings += {
							{ "name", "enabled" },
							{ "label", "Enabled" },
							{ "type", "boolean" },
							{ "default", false },
							{ "sort", 2 }
						};
						settings += {
							{ "name", "type" },
							{ "label", "Type" },
							{ "type", "list" },
							{ "mandatory", true },
							{ "options", {
#ifdef _DEBUG
								{
									{ "value", "debug" },
									{ "label", Debug::label }
								},
#endif // _DEBUG
								{
									{ "value", "dummy" },
									{ "label", Dummy::label }
								},
								{
									{ "value", "harmony_hub" },
									{ "label", HarmonyHub::label },
									{ "settings", HarmonyHub::getEmptySettingsJson() }
								},
#ifdef _WITH_LINUX_SPI
								{
									{ "value", "piface" },
									{ "label", PiFace::label }
								},
#endif // _WITH_LINUX_SPI
								{
									{ "value", "rfxcom" },
									{ "label", RFXCom::label },
									{ "settings", RFXCom::getEmptySettingsJson() }
								},
								{
									{ "value", "solaredge" },
									{ "label", SolarEdge::label },
									{ "settings", SolarEdge::getEmptySettingsJson() }
								},
								{
									{ "value", "telegram" },
									{ "label", Telegram::label },
									{ "settings", Telegram::getEmptySettingsJson() }
								},
								{
									{ "value", "weather_underground" },
									{ "label", WeatherUnderground::label },
									{ "settings", WeatherUnderground::getEmptySettingsJson() }
								},
#ifdef _WITH_OPENZWAVE
								{
									{ "value", "zwave" },
									{ "label", ZWave::label },
									{ "settings", ZWave::getEmptySettingsJson() }
								},
#endif // _WITH_OPENZWAVE
							} },
							{ "sort", 1 }
						};
						return settings;
					} else {
						return plugin_->getSettingsJson();
					}
				};

				switch( method_ ) {
					case WebServer::Method::GET: {
						auto find = input_.find( "$2" );
						if ( __unlikely( find != input_.end() ) ) {
							auto pluginIds = stringSplit( jsonGet<>( *find ), ',' );
							if ( pluginIds[0] == "settings" ) {
								output_["data"] = fGetSettings( nullptr );
							} else {
								if ( pluginIds.size() == 1 ) {
									std::shared_ptr<Plugin> plugin = g_controller->getPluginById( std::stoi( pluginIds[0] ) );
									if ( plugin ) {
										output_["data"] = plugin->getJson();
										output_["data"]["settings"] = fGetSettings( plugin );
										auto children = plugin->getChildren();
										if ( children.size() > 0 ) {
											output_["data"]["children"] = json::array();
											for ( auto& child : children ) {
												output_["data"]["children"] += child->getJson();
											}
										}
										auto parent = plugin->getParent();
										if ( parent != nullptr ) {
											output_["data"]["parent"] = parent->getJson();
										}
									} else {
										return; // 404
									}
								} else {
									output_["data"] = json::array();
									for ( auto& pluginId : pluginIds ) {
										std::shared_ptr<Plugin> plugin = g_controller->getPluginById( std::stoi( pluginId ) );
										if ( plugin != nullptr ) {
											output_["data"] += plugin->getJson();
										} else {
											return; // 404
										}
									}
								}
							}
						} else {
							output_["data"] = json::array();
							for ( auto& plugin : g_controller->getAllPlugins() ) {
								if ( plugin->getParent() == nullptr ) {
									output_["data"] += plugin->getJson();
								}
							}
						}
						output_["code"] = 200;
						break;
					}

					case WebServer::Method::DELETE: {
						auto find = input_.find( "$2" );
						if ( __likely( find != input_.end() ) ) {
							auto pluginIds = stringSplit( jsonGet<>( *find ), ',' );
							std::vector<std::shared_ptr<Plugin>> plugins;
							for ( auto& pluginId : pluginIds ) {
								std::shared_ptr<Plugin> plugin = g_controller->getPluginById( std::stoi( pluginId ) );
								if ( plugin != nullptr ) {
									plugins.push_back( plugin );
								} else {
									return; // 404
								}
							}
							for ( auto& plugin : plugins ) {
								g_controller->removePlugin( plugin );
							}
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						auto find = input_.find( "$2" );
						if (
							method_ == WebServer::Method::POST
							&& find != input_.end()
						) {
							return; // 404
						} else if (
							method_ == WebServer::Method::PUT
							&& find == input_.end()
						) {
							return; // 404
						}
						std::shared_ptr<Plugin> plugin = nullptr;
						if ( find != input_.end() ) {
							auto pluginIds = stringSplit( jsonGet<>( *find ), ',' );
							if (
								pluginIds.size() > 1
								|| ! ( plugin = g_controller->getPluginById( std::stoi( pluginIds[0] ) ) )
							) {
								return; // 404
							}
						}

						std::vector<std::string> invalid;
						std::vector<std::string> missing;
						std::vector<std::string> errors;
						json pluginData = json::object();
						WebServer::_validateSettings( input_, pluginData, fGetSettings( plugin ), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Plugin.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Plugin.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						if ( ! plugin ) {
							Plugin::Type type = Plugin::resolveTextType( jsonGet<>( pluginData, "type" ) );
							std::string reference = randomString( 16 );
							bool enabled = jsonGet<bool>( pluginData, "enabled" );
							plugin = g_controller->declarePlugin( type, reference, { }, enabled );

							// Remove the data that shouldn't end-up in settings and store the rest.
							pluginData.erase( "type" );
							pluginData.erase( "enabled" );
							plugin->getSettings()->put( pluginData );
							plugin->getSettings()->commit();
							if ( enabled ) {
								this->m_scheduler.schedule( 0, 1, this, [plugin]( std::shared_ptr<Scheduler::Task<>> ) {
									plugin->start();
								} );
							}

							output_["data"] = { "id", plugin->getId() };
							output_["code"] = 201; // Created
						} else {
							// The enabled property can be used to enable or disable the plugins. For now this is only
							// possible on parent/main plugin, no children.
							bool enabled = ( plugin->getState() != Plugin::State::DISABLED );
							if ( plugin->getParent() == nullptr ) {
								enabled = jsonGet<bool>( pluginData, "enabled" );
								pluginData.erase( "enabled" );

								g_database->putQuery(
									"UPDATE `plugins` "
									"SET `enabled`=%d "
									"WHERE `id`=%d "
									"OR `plugin_id`=%d", // also children
									enabled ? 1 : 0,
									plugin->getId(),
									plugin->getId()
								);
							}

							plugin->putSettingsJson( pluginData );
							plugin->getSettings()->put( pluginData );
							bool restart = false;
							if ( plugin->getSettings()->isDirty() ) {
								plugin->getSettings()->commit();
								// Only parent plugins are restarted. Child plugins needs to be restarted after an
								// update it should listen for putSettingsJon.
								if ( plugin->getParent() == nullptr ) {
									restart = true;
								}
							}

							this->m_scheduler.schedule( 0, 1, this, [=]( std::shared_ptr<Scheduler::Task<>> ) {
								auto plugins = g_controller->getAllPlugins();
								if (
									! enabled
									|| restart
								) {
									if ( plugin->getState() != Plugin::State::DISABLED ) {
										plugin->stop();
									}
									for ( auto& pluginIt : plugins ) {
										if (
											pluginIt->getParent() == plugin
											&& pluginIt->getState() != Plugin::State::DISABLED
										) {
											pluginIt->stop();
										}
									}
								}
								if ( enabled ) {
									this->m_scheduler.schedule( SCHEDULER_INTERVAL_1SEC, 1, this, [plugin]( std::shared_ptr<Scheduler::Task<>> ) {
										if ( plugin->getState() == Plugin::State::DISABLED ) {
											plugin->start();
										}
									} );
								}
							} );
							output_["code"] = 200;
						}
						break;
					}

					default: break;
				}
			}
		};
	};

	void WebServer::_installDeviceResourceHandler() {
		this->m_resources[1] = {
			"^/api(/(plugins|scripts)/([0-9]+))?/devices(/([0-9,]+))?$",
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::VIEWER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				std::shared_ptr<Plugin> plugin = nullptr;
				int scriptId = -1;
				auto find = input_.find( "$2" );
				if ( find != input_.end() ) {
					if ( jsonGet<>( *find ) == "plugins" ) {
						find = input_.find( "$3" );
						if ( __unlikely( nullptr == ( plugin = g_controller->getPluginById( jsonGet<int>( *find ) ) ) ) ) {
							return;
						} else if ( user_->getRights() < User::Rights::INSTALLER ) {
							throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
						}
					} else {
						find = input_.find( "$3" );
						if ( __unlikely( g_database->getQueryValue<unsigned int>( "SELECT COUNT(*) FROM `scripts` WHERE `id`=%d", jsonGet<unsigned int>( *find ) ) == 0 ) ) {
							return;
						} else {
							scriptId = jsonGet<unsigned int>( *find );
						}
					}
				}

				switch( method_ ) {
					case WebServer::Method::GET: {
						auto find = input_.find( "$5" );
						if ( __unlikely( find != input_.end() ) ) {
							if ( scriptId > -1 ) {
								return;
							}
							auto deviceIds = stringSplit( jsonGet<>( *find ), ',' );
							if ( deviceIds.size() == 1 ) {
								std::shared_ptr<Device> device = nullptr;
								if ( plugin != nullptr ) {
									device = plugin->getDeviceById( std::stoi( deviceIds[0] ) );
								} else {
									device = g_controller->getDeviceById( std::stoi( deviceIds[0] ) );
								}
								if ( device ) {
									output_["data"] = device->getJson();
									if ( user_->getRights() >= User::Rights::INSTALLER ) {
										output_["data"]["settings"] = device->getSettingsJson();
										output_["data"]["scripts"] = g_database->getQueryColumn<unsigned int>(
											"SELECT s.`id` "
											"FROM `scripts` s, `x_device_scripts` x "
											"WHERE s.`id`=x.`script_id` "
											"AND x.`device_id`=%d "
											"ORDER BY s.`id` ASC",
											device->getId()
										);
										output_["data"]["links"] = g_database->getQueryColumn<unsigned int>(
											"SELECT DISTINCT CASE WHEN l.`device_id`=%d THEN l.`target_device_id` ELSE l.`device_id` END "
											"FROM `links` l "
											"WHERE l.`device_id`=%d "
											"OR l.`target_device_id`=%d ",
											device->getId(),
											device->getId(),
											device->getId()
										);
									}
								} else {
									return; // 404
								}
							} else {
								output_["data"] = json::array();
								for ( auto& deviceId : deviceIds ) {
									std::shared_ptr<Device> device = nullptr;
									if ( plugin != nullptr ) {
										device = plugin->getDeviceById( std::stoi( deviceId ) );
									} else {
										device = g_controller->getDeviceById( std::stoi( deviceId ) );
									}
									if ( device != nullptr ) {
										output_["data"] += device->getJson();
									} else {
										return; // 404
									}
								}
							}
						} else {
							output_["data"] = json::array();
							if ( plugin != nullptr ) {
								for ( auto& device : plugin->getAllDevices() ) {
									output_["data"] += device->getJson();
								}
							} else if ( scriptId > -1 ) {
								auto deviceIds = g_database->getQueryColumn<unsigned int>(
									"SELECT DISTINCT `device_id` "
									"FROM `x_device_scripts` "
									"WHERE `script_id`=%d "
									"ORDER BY `device_id` ASC",
									scriptId
								);
								for ( auto& deviceId : deviceIds ) {
									output_["data"] += g_controller->getDeviceById( deviceId )->getJson();
								}
							} else {
								for ( auto& device : g_controller->getAllDevices() ) {
									if ( device->isEnabled() ) {
										output_["data"] += device->getJson();
									}
								}
							}
						}
						output_["code"] = 200;
						break;
					}

					case WebServer::Method::DELETE: {
						auto find = input_.find( "$5" );
						if ( __likely( find != input_.end() ) ) {
							auto deviceIds = stringSplit( jsonGet<>( *find ), ',' );
							std::vector<std::shared_ptr<Device>> devices;
							for ( auto& deviceId : deviceIds ) {
								std::shared_ptr<Device> device = nullptr;
								if ( plugin != nullptr ) {
									device = plugin->getDeviceById( std::stoi( deviceId ) );
								} else {
									device = g_controller->getDeviceById( std::stoi( deviceId ) );
								}
								if ( device != nullptr ) {
									devices.push_back( device );
								} else {
									return; // 404
								}
							}
							for ( auto& device : devices ) {
								device->getPlugin()->removeDevice( device );
							}
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT: {
						auto find = input_.find( "$5" );
						if (
							user_->getRights() >= User::Rights::INSTALLER
							&& find != input_.end()
						) {
							std::shared_ptr<Device> device = nullptr;
							auto deviceIds = stringSplit( jsonGet<>( *find ), ',' );
							if ( deviceIds.size() == 1 ) {
								if ( plugin != nullptr ) {
									device = plugin->getDeviceById( std::stoi( deviceIds[0] ) );
								} else {
									device = g_controller->getDeviceById( std::stoi( deviceIds[0] ) );
								}
								if ( ! device ) {
									return; // 404
								}
							} else {
								return; // 404
							}

							std::vector<std::string> invalid;
							std::vector<std::string> missing;
							std::vector<std::string> errors;
							json deviceData = json::object();
							WebServer::_validateSettings( input_, deviceData, device->getSettingsJson(), &invalid, &missing, &errors );
							if ( invalid.size() > 0 ) {
								throw WebServer::ResourceException( 400, "Device.Invalid.Fields", stringJoin( errors, ", " ) );
							} else if ( missing.size() > 0 ) {
								throw WebServer::ResourceException( 400, "Device.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
							}

							bool enabled = jsonGet<bool>( deviceData, "enabled" );
							deviceData.erase( "enabled" );
							if ( enabled != device->isEnabled() ) {
								if ( enabled ) {
									device->setEnabled( enabled );
									device->start();
								} else {
									device->stop();
									device->setEnabled( enabled );
								}
							}

							g_database->putQuery(
								"UPDATE `devices` "
								"SET `enabled`=%d "
								"WHERE `id`=%d",
								enabled ? 1 : 0,
								device->getId()
							);

							// A scripts array can be passed along to set the scripts to run when the device is updated.
							auto find = deviceData.find( "scripts");
							if ( find != deviceData.end() ) {
								auto scripts = jsonGet<std::vector<unsigned int>>( *find );
								device->setScripts( scripts );
								deviceData.erase( find );
							}

							// Every other setting is stored in the device_settings table using the settings class.
							device->putSettingsJson( deviceData );
							device->getSettings()->put( deviceData );
							if ( device->getSettings()->isDirty() ) {
								device->getSettings()->commit();
							}

							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PATCH: {
						auto find = input_.find( "$5" );
						if ( find != input_.end() ) {
							std::shared_ptr<Device> device = nullptr;
							auto deviceIds = stringSplit( jsonGet<>( *find ), ',' );
							if ( deviceIds.size() == 1 ) {
								if ( plugin != nullptr ) {
									device = plugin->getDeviceById( std::stoi( deviceIds[0] ) );
								} else {
									device = g_controller->getDeviceById( std::stoi( deviceIds[0] ) );
								}
								if ( ! device ) {
									return; // 404
								}
							} else {
								return; // 404
							}

							if ( user_->getRights() >= device->getSettings()->get<User::Rights>( DEVICE_SETTING_MINIMUM_USER_RIGHTS, User::Rights::USER ) ) {
								try {
									switch( device->getType() ) {
										case Device::Type::COUNTER:
											device->updateValue<Counter>( Device::UpdateSource::API, jsonGet<int>( input_, "value" ) );
											break;
										case Device::Type::LEVEL:
											device->updateValue<Level>( Device::UpdateSource::API, jsonGet<double>( input_, "value" ) );
											break;
										case Device::Type::SWITCH:
										case Device::Type::TEXT:
											device->updateValue<Switch>( Device::UpdateSource::API, jsonGet<>( input_, "value" ) );
											break;
									}
								} catch( std::runtime_error exception_ ) {
									throw WebServer::ResourceException( 400, "Device.Invalid.Value", "The supplied value is invalid." );
								}

								output_["code"] = 200;
							} else {
								return; // 404
							}
						}
						break;
					}

					default: break;
				}
			}
		};

		this->m_resources[2] = {
			"^/api/devices/([0-9]+)/data$",
			WebServer::Method::GET | WebServer::Method::POST,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::VIEWER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				std::shared_ptr<Device> device = nullptr;
				int deviceId = -1;
				auto find = input_.find( "$1" );
				if ( find != input_.end() ) {
					deviceId = jsonGet<int>( *find );
					if ( nullptr == ( device = g_controller->getDeviceById( deviceId ) ) ) {
						return;
					}
				}

				try {
					switch( device->getType() ) {
						case Device::Type::SWITCH:
							output_["data"] = std::static_pointer_cast<Switch>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<>( input_, "interval" ) );
							break;
						case Device::Type::COUNTER:
							output_["data"] = std::static_pointer_cast<Counter>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<>( input_, "interval" ), jsonGet<>( input_, "group" ) );
							break;
						case Device::Type::LEVEL:
							output_["data"] = std::static_pointer_cast<Level>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<>( input_, "interval" ), jsonGet<>( input_, "group" ) );
							break;
						case Device::Type::TEXT:
							output_["data"] = std::static_pointer_cast<Text>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<>( input_, "interval" ) );
							break;
					}
				} catch( ... ) {
					throw WebServer::ResourceException( 400, "Device.Invalid.Parameters", "The supplied parameters are invalid." );
				}

				output_["code"] = 200;
			}
		};
	};

	void WebServer::_installLinkResourceHandler() {
		this->m_resources[3] = {
			"^/api(/devices/([0-9]+))?/links(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::POST | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::INSTALLER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				int deviceId = -1;
				auto find = input_.find( "$1" );
				if (
					find != input_.end()
					&& jsonGet<>( *find ).substr( 0, 8 ) == "/devices"
				) {
					deviceId = jsonGet<int>( input_, "$2" );
				}

				auto fGetSettings = [deviceId]() -> json {
					auto devices  = g_controller->getAllDevices();
					json sourceDevices = json::array();
					json targetDevices = json::array();
					for ( auto devicesIt = devices.begin(); devicesIt != devices.end(); devicesIt++ ) {
						auto updateSources = (*devicesIt)->getSettings()->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES );
						if (
							(*devicesIt)->isEnabled()
							&& (*devicesIt)->getType() == Device::Type::SWITCH
						) {
							sourceDevices += {
								{ "value", (*devicesIt)->getId() },
								{ "label", (*devicesIt)->getName() }
							};
							if ( ( updateSources & Device::UpdateSource::LINK ) == Device::UpdateSource::LINK ) {
								targetDevices += {
									{ "value", (*devicesIt)->getId() },
									{ "label", (*devicesIt)->getName() }
								};
							}
						}
					}

					json switchOptions = json::array();
					for ( auto switchOptionIt = Switch::OptionText.begin(); switchOptionIt != Switch::OptionText.end(); switchOptionIt++ ) {
						switchOptions += {
							{ "value", (*switchOptionIt).second },
							{ "label", (*switchOptionIt).second }
						};
					}

					json settings = json::array();
					settings += {
						{ "name", "name" },
						{ "default", "New Link" },
						{ "label", "Name" },
						{ "type", "string" },
						{ "maxlength", 64 },
						{ "minlength", 3 },
						{ "mandatory", true },
						{ "sort", 1 }
					};
					settings += {
						{ "name", "enabled" },
						{ "label", "Enabled" },
						{ "type", "boolean" },
						{ "mandatory", true },
						{ "default", true },
						{ "sort", 2 }
					};
					settings += {
						{ "name", "device_id" },
						{ "label", "Source Device" },
						{ "type", "list" },
						{ "mandatory", true },
						{ "options", sourceDevices },
						{ "sort", 10 }
					};
					settings += {
						{ "name", "target_device_id" },
						{ "label", "Target Device" },
						{ "type", "list" },
						{ "mandatory", true },
						{ "options", targetDevices },
						{ "sort", 11 }
					};
					settings += {
						{ "name", "value" },
						{ "label", "Value" },
						{ "description", "The value that triggers this link." },
						{ "default", "On" },
						{ "mandatory", true },
						{ "type", "list" },
						{ "options", switchOptions },
						{ "sort", 12 }
					};
					settings += {
						{ "name", "target_value" },
						{ "label", "Target Value" },
						{ "description", "The value to set the target device to when the link is triggered." },
						{ "default", "On" },
						{ "mandatory", true },
						{ "type", "list" },
						{ "options", switchOptions },
						{ "sort", 13 }
					};
					settings += {
						{ "name", "after" },
						{ "label", "Delay" },
						{ "description", "Sets the delay in seconds after which the value will be set." },
						{ "type", "double" },
						{ "minimum", 0 },
						{ "maximum", 86400 },
						{ "class", "advanced" },
						{ "sort", 20 }
					};
					settings += {
						{ "name", "for" },
						{ "label", "Duration" },
						{ "description", "Sets the duration in seconds after which the value will be set back to it's previous value." },
						{ "type", "double" },
						{ "minimum", 0 },
						{ "maximum", 86400 },
						{ "class", "advanced" },
						{ "sort", 21 }
					};
					settings += {
						{ "name", "clear" },
						{ "label", "Clear Queue" },
						{ "description", "Clears the event queue of events for this device, making sure that the value to set is the only value present in the queue." },
						{ "default", false },
						{ "type", "boolean" },
						{ "class", "advanced" },
						{ "sort", 22 }
					};
					return settings;
				};

				json link = json::object();
				int linkId = -1;
				find = input_.find( "$4" );
				if ( find != input_.end() ) {
					try {
						if (
							"settings" == jsonGet<>( *find )
							&& method_ == WebServer::Method::GET
						) {
							output_["data"] = fGetSettings();
							output_["code"] = 200;
							return;
						} else {
							linkId = jsonGet<int>( *find );
							link = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `device_id`, `target_device_id`, `value`, `target_value`, `after`, `for`, `clear`, `enabled` "
								"FROM `links` "
								"WHERE `id`=%d ",
								linkId
							);
						}
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( __likely( linkId != -1 ) ) {
							output_["data"] = link;
							output_["data"]["settings"] = fGetSettings();
						} else {
							if ( __likely( deviceId != -1 ) ) {
								output_["data"] = g_database->getQuery<json>(
									"SELECT `id`, `name`, `device_id`, `target_device_id`, `value`, `target_value`, `after`, `for`, `clear`, `enabled` "
									"FROM `links` "
									"WHERE `device_id`=%d "
									"OR `target_device_id`=%d "
									"ORDER BY `name`",
									deviceId,
									deviceId
								);
							} else {
								output_["data"] = g_database->getQuery<json>(
									"SELECT `id`, `name`, `device_id`, `target_device_id`, `value`, `target_value`, `after`, `for`, `clear`, `enabled` "
									"FROM `links` "
									"ORDER BY `name`"
								);
							}
							// Add the names of the devices involved to the list. This prevents a client from having to
							// fetch the devices separately, only to show this list.
							for ( auto dataIt = output_["data"].begin(); dataIt != output_["data"].end(); dataIt++ ) {
								(*dataIt)["device"] = g_controller->getDeviceById( (*dataIt)["device_id"].get<unsigned int>() )->getName();
								(*dataIt)["target_device"] = g_controller->getDeviceById( (*dataIt)["target_device_id"].get<unsigned int>() )->getName();
							}
						}
						output_["code"] = 200;
						break;
					}
					case WebServer::Method::DELETE: {
						if ( linkId != -1 ) {
							g_database->putQuery(
								"DELETE FROM `links` "
								"WHERE `id`=%d",
								linkId
							);
							output_["code"] = 200;
						}
						break;
					}
					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						if (
							method_ == WebServer::Method::POST
							&& linkId != -1
						) {
							break;
						} else if (
							method_ == WebServer::Method::PUT
							&& linkId == -1
						) {
							break;
						}

						std::vector<std::string> invalid;
						std::vector<std::string> missing;
						std::vector<std::string> errors;
						json linkData = json::object();
						WebServer::_validateSettings( input_, linkData, fGetSettings(), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Link.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Link.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						if ( linkId == -1 ) {
							linkId = g_database->putQuery(
								"INSERT INTO `links` ( `name`, `device_id`, `target_device_id`, `enabled`, `value`, `target_value`, `after`, `for`, `clear` ) "
								"VALUES ( %Q, %d, %d, %d, %Q, %Q, %Q, %Q, %d )",
								linkData["name"].get<std::string>().c_str(),
								linkData["device_id"].get<unsigned int>(),
								linkData["target_device_id"].get<unsigned int>(),
								linkData["enabled"].get<bool>() ? 1 : 0,
								linkData["value"].is_null() ? NULL : linkData["value"].get<std::string>().c_str(),
								linkData["target_value"].is_null() ? NULL : linkData["target_value"].get<std::string>().c_str(),
								linkData["after"].is_null() ? NULL : std::to_string( linkData["after"].get<double>() ).c_str(),
								linkData["for"].is_null() ? NULL : std::to_string( linkData["for"].get<double>() ).c_str(),
								linkData["clear"].is_null() ? 0 : ( linkData["clear"].get<bool>() ? 1 : 0 )
							);
							output_["data"] = { "id", linkId };
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `links` "
								"SET `name`=%Q, `device_id`=%d, `target_device_id`=%d, `enabled`=%d, `value`=%Q, `target_value`=%Q, `after`=%Q, `for`=%Q, `clear`=%d "
								"WHERE `id`=%d",
								linkData["name"].get<std::string>().c_str(),
								linkData["device_id"].get<unsigned int>(),
								linkData["target_device_id"].get<unsigned int>(),
								linkData["enabled"].get<bool>() ? 1 : 0,
								linkData["value"].is_null() ? NULL : linkData["value"].get<std::string>().c_str(),
								linkData["target_value"].is_null() ? NULL : linkData["target_value"].get<std::string>().c_str(),
								linkData["after"].is_null() ? NULL : std::to_string( linkData["after"].get<double>() ).c_str(),
								linkData["for"].is_null() ? NULL : std::to_string( linkData["for"].get<double>() ).c_str(),
								linkData["clear"].is_null() ? 0 : ( linkData["clear"].get<bool>() ? 1 : 0 ),
								linkId
							);
							output_["code"] = 200;
						}
						break;
					}
					default: break;
				}
			}
		};
	};

	void WebServer::_installScriptResourceHandler() {
		this->m_resources[4] = {
			"^/api/scripts(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::INSTALLER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				// This helper method returns a list of settings that can be used to edit a new or existing script.
				auto fGetSettings = []() -> json {
					json settings = json::array();
					settings += {
						{ "name", "name" },
						{ "label", "Name" },
						{ "type", "string" },
						{ "maxlength", 64 },
						{ "minlength", 3 },
						{ "mandatory", true },
						{ "default", "New Script" },
						{ "sort", 1 }
					};
					settings += {
						{ "name", "code" },
						{ "label", "Code" },
						{ "type", "string" },
						{ "mandatory", true },
						{ "default", "" },
						{ "sort", 2 }
					};
					settings += {
						{ "name", "enabled" },
						{ "label", "Enabled" },
						{ "type", "boolean" },
						{ "mandatory", true },
						{ "default", true },
						{ "sort", 3 }
					};
					return settings;
				};

				json script = json::object();
				int scriptId = -1;
				auto find = input_.find( "$2" );
				if ( find != input_.end() ) {
					try {
						if (
							"settings" == jsonGet<>( *find )
							&& method_ == WebServer::Method::GET
						) {
							output_["data"] = fGetSettings();
							output_["code"] = 200;
							return;
						} else {
							scriptId = jsonGet<int>( *find );
							script = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `code`, `enabled` "
								"FROM `scripts` "
								"WHERE `id`=%d ",
								scriptId
							);
						}
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( scriptId != -1 ) {
							output_["data"] = script;
							output_["data"]["settings"] = fGetSettings();
							output_["data"]["device_ids"] = g_database->getQueryColumn<unsigned int>(
								"SELECT DISTINCT `device_id` "
								"FROM `x_device_scripts` "
								"WHERE `script_id`=%d "
								"ORDER BY `device_id` ASC",
								script["id"].get<unsigned int>()
							);
						} else {
							output_["data"] = g_database->getQuery<json>(
								"SELECT `id`, `name`, `code`, `enabled` "
								"FROM `scripts` "
								"ORDER BY `id` ASC"
							);
						}
						output_["code"] = 200;
						break;
					}

					case WebServer::Method::DELETE: {
						if ( scriptId != -1 ) {
							g_database->putQuery(
								"DELETE FROM `scripts` "
								"WHERE `id`=%d",
								scriptId
							);
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						if (
							method_ == WebServer::Method::POST
							&& scriptId != -1
						) {
							break;
						} else if (
							method_ == WebServer::Method::PUT
							&& scriptId == -1
						) {
							break;
						}

						std::vector<std::string> invalid;
						std::vector<std::string> missing;
						std::vector<std::string> errors;
						json scriptData = json::object();
						WebServer::_validateSettings( input_, scriptData, fGetSettings(), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Script.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Script.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						if ( scriptId == -1 ) {
							scriptId = g_database->putQuery(
								"INSERT INTO `scripts` (`name`, `code`, `enabled`) "
								"VALUES (%Q, %Q, %d) ",
								scriptData["name"].get<std::string>().c_str(),
								scriptData["code"].get<std::string>().c_str(),
								scriptData["enabled"].get<bool>() ? 1 : 0
							);
							output_["data"] = { "id", scriptId };
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `scripts` "
								"SET `name`=%Q, `code`=%Q, `enabled`=%d "
								"WHERE `id`=%d",
								scriptData["name"].get<std::string>().c_str(),
								scriptData["code"].get<std::string>().c_str(),
								scriptData["enabled"].get<bool>() ? 1 : 0,
								scriptId
							);
							output_["code"] = 200;
						}
						break;
					}

					default: break;
				}
			}
		};
	};

	void WebServer::_installTimerResourceHandler() {
		this->m_resources[5] = {
			"^/api(/devices/([0-9]+))?/timers(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::INSTALLER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				// Timers can be associated with scripts or devices. If a timer is associated with a device, the url
				// should contain the device.
				int deviceId = -1;
				auto find = input_.find( "$1" );
				if (
					find != input_.end()
					&& jsonGet<>( *find ).substr( 0, 8 ) == "/devices"
				) {
					deviceId = jsonGet<int>( input_, "$2" );
				}

				// This helper method returns a list of settings that can be used to edit a new or existing timer.
				auto fGetSettings = [deviceId]() -> json {
					json settings = json::array();
					settings += {
						{ "name", "name" },
						{ "label", "Name" },
						{ "type", "string" },
						{ "maxlength", 64 },
						{ "minlength", 3 },
						{ "mandatory", true },
						{ "default", "New Timer" },
						{ "sort", 1 }
					};
					settings += {
						{ "name", "cron" },
						{ "label", "Cron" },
						{ "type", "string" },
						{ "maxlength", 64 },
						{ "minlength", 1 },
						{ "mandatory", true },
						{ "default", "* * * * *" },
						{ "description", "To define the time you can provide concrete values for minute (m), hour (h), day of month (dom), month (mon), and day of week (dow). Use an asterisk in these fields for any. Lists (comma), ranges (dash) and steps (forward slash) are also supported." },
						{ "sort", 2 }
					};
					settings += {
						{ "name", "enabled" },
						{ "label", "Enabled" },
						{ "type", "boolean" },
						{ "mandatory", true },
						{ "default", true },
						{ "sort", 3 }
					};
					if ( __likely( deviceId != -1 ) ) {
						auto device = g_controller->getDeviceById( deviceId );
						switch( device->getType() ) {
							case Device::Type::COUNTER:
							case Device::Type::LEVEL: {
								settings += {
									{ "name", "value" },
									{ "label", "Value" },
									{ "type", "double" },
									{ "mandatory", true },
									{ "sort", 4 }
								};
								break;
							}
							case Device::Type::SWITCH: {
								json switchOptions = json::array();
								for ( auto switchOptionIt = Switch::OptionText.begin(); switchOptionIt != Switch::OptionText.end(); switchOptionIt++ ) {
									switchOptions += {
										{ "value", (*switchOptionIt).second },
										{ "label", (*switchOptionIt).second }
									};
								}
								settings += {
									{ "name", "value" },
									{ "label", "Value" },
									{ "default", "On" },
									{ "mandatory", true },
									{ "type", "list" },
									{ "options", switchOptions },
									{ "sort", 4 }
								};
								break;
							}
							case Device::Type::TEXT: {
								settings += {
									{ "name", "value" },
									{ "label", "Value" },
									{ "type", "string" },
									{ "mandatory", true },
									{ "sort", 4 }
								};
								break;
							}
						}
					} else {
						json scriptOptions = json::array();
						auto scripts = g_database->getQuery(
							"SELECT s.`id`, s.`name` "
							"FROM `scripts` s LEFT JOIN `x_timer_scripts` x "
							"ON s.`id`=x.`script_id` "
							"WHERE s.`enabled`=1 "
							"OR x.`timer_id` IS NOT NULL "
							"ORDER BY s.`name`"
						);
						for ( auto &script : scripts ) {
							scriptOptions += {
								{ "value", std::stoi( script["id"] ) },
								{ "label", script["name"] }
							};
						}
						if ( scriptOptions.size() > 0 ) {
							settings += {
								{ "name", "scripts" },
								{ "label", "Scripts" },
								{ "type", "multiselect" },
								{ "options", scriptOptions },
								{ "sort", 4 }
							};
						}
					}
					return settings;
				};

				json timer = json::object();
				int timerId = -1;
				find = input_.find( "$4" );
				if ( find != input_.end() ) {
					try {
						if (
							"settings" == jsonGet<>( *find )
							&& method_ == WebServer::Method::GET
						) {
							output_["data"] = fGetSettings();
							output_["code"] = 200;
							return;
						} else {
							timerId = jsonGet<int>( *find );
							timer = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `cron`, `enabled` "
								"FROM `timers` "
								"WHERE `id`=%d ",
								timerId
							);
						}
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( __likely( timerId != -1 ) ) {
							output_["data"] = timer;
							output_["data"]["settings"] = fGetSettings();
							if ( __likely( deviceId != -1 ) ) {
								output_["data"]["value"] = g_database->getQueryValue<std::string>(
									"SELECT `value` "
									"FROM `x_timer_devices` "
									"WHERE `timer_id`=%d "
									"AND `device_id`=%d",
									timerId,
									deviceId
								);
							} else {
								output_["data"]["scripts"] = g_database->getQueryColumn<unsigned int>(
									"SELECT s.`id` "
									"FROM `scripts` s, `x_timer_scripts` x "
									"WHERE s.`id`=x.`script_id` "
									"AND x.`timer_id`=%d "
									"ORDER BY s.`id` ASC",
									timerId
								);
							}
						} else {
							if ( __likely( deviceId != -1 ) ) {
								output_["data"] = g_database->getQuery<json>(
									"SELECT t.`id`, t.`name`, t.`cron`, t.`enabled`, x.`value` "
									"FROM `timers` t, `x_timer_devices` x "
									"WHERE t.`id`=x.`timer_id` "
									"AND x.`device_id`=%d "
									"ORDER BY t.`id` ASC",
									deviceId
								);
							} else {
								output_["data"] = g_database->getQuery<json>(
									"SELECT t.`id`, t.`name`, t.`cron`, t.`enabled` "
									"FROM `timers` t LEFT JOIN `x_timer_devices` x "
									"ON t.`id`=x.`timer_id` "
									"WHERE x.`device_id` IS NULL "
									"ORDER BY t.`id` ASC"
								);
							}
						}
						output_["code"] = 200;
						break;
					}

					case WebServer::Method::DELETE: {
						if ( timerId != -1 ) {
							g_database->putQuery(
								"DELETE FROM `timers` "
								"WHERE `id`=%d",
								timerId
							);
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						if (
							method_ == WebServer::Method::POST
							&& timerId != -1
						) {
							break;
						} else if (
							method_ == WebServer::Method::PUT
							&& timerId == -1
						) {
							break;
						}

						std::vector<std::string> invalid;
						std::vector<std::string> missing;
						std::vector<std::string> errors;
						json timerData = json::object();
						WebServer::_validateSettings( input_, timerData, fGetSettings(), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Timer.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Timer.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						if ( timerId == -1 ) {
							timerId = g_database->putQuery(
								"INSERT INTO `timers` (`name`, `cron`, `enabled`) "
								"VALUES (%Q, %Q, %d) ",
								timerData["name"].get<std::string>().c_str(),
								timerData["cron"].get<std::string>().c_str(),
								timerData["enabled"].get<bool>() ? 1 : 0
							);
							output_["data"] = { "id", timerId };
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `timers` "
								"SET `name`=%Q, `cron`=%Q, `enabled`=%d "
								"WHERE `id`=%d",
								timerData["name"].get<std::string>().c_str(),
								timerData["cron"].get<std::string>().c_str(),
								timerData["enabled"].get<bool>() ? 1 : 0,
								timerId
							);
							output_["code"] = 200;
						}

						if ( __likely( deviceId != -1 ) ) {
							g_database->putQuery(
								"REPLACE INTO `x_timer_devices` "
								"( `timer_id`, `device_id`, `value` ) "
								"VALUES ( %d, %d, %Q )",
								timerId,
								deviceId,
								timerData["value"].get<std::string>().c_str()
							);
						} else {
							auto scripts = jsonGet<std::vector<int>>( timerData, "scripts" );
							for ( auto &scriptId : scripts ) {
								g_database->putQuery(
									"INSERT OR IGNORE INTO `x_timer_scripts` "
									"( `timer_id`, `script_id` ) "
									"VALUES ( %d, %d )",
									timerId,
									scriptId
								);
							}
							g_database->putQuery(
								"DELETE FROM `x_timer_scripts` "
								"WHERE `timer_id` = %d "
								"AND `script_id` NOT IN ( %q )",
								timerId,
								jsonGet<>( timerData, "scripts" ).c_str()
							);
						}
					}

					default: break;
				}
			}
		};
	};

	void WebServer::_installUserResourceHandler() {
		this->m_resources[6] = {
			"^/api/user/(login|refresh)$",
			WebServer::Method::GET | WebServer::Method::POST,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_loginsMutex );
				if (
					method_ == WebServer::Method::POST
					&& input_["$1"].get<std::string>() == "login"
				) {
					// The _processHttpRequest has already checked the username and password, so if no user pas passed
					// to this callback it means that either the username or password was invalid.
					if ( user_ == nullptr ) {
						Logger::log( Logger::LogLevel::WARNING, this, "Invalid username and/or password supplied." );
						throw WebServer::ResourceException( 401, "Login.Failure", "The username and/or password is invalid." );
					}
					output_["code"] = 201; // Created
					Logger::logr( Logger::LogLevel::NORMAL, this, "User %s logged in.", user_->getName().c_str() );
				} else if (
					method_ == WebServer::Method::GET
					&& input_["$1"].get<std::string>() == "refresh"
				) {
					if ( user_ != nullptr ) {
						// After a login token has been refreshed, the old token should be expired, just not immediately
						// because there might be concurrent requests using the old token that need to be able to finish.
						this->m_logins.at( jsonGet<>( input_, "_token" ) ).valid = system_clock::now() + minutes( 1 );
						output_["code"] = 200; // Refreshed
						Logger::logr( Logger::LogLevel::NORMAL, this, "User %s prolonged login.", user_->getName().c_str() );
					} else {
						throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
					}
				} else {
					return;
				}

				std::string token = randomString( 32 );
				system_clock::time_point valid = system_clock::now() + minutes( WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES );
				this->m_logins[token] = { valid, user_ };

				output_["data"] = {
					{ "user", {
						{ "id", user_->getId() },
						{ "name", user_->getName() },
						{ "rights", user_->getRights() }
					} },
					{ "valid", duration_cast<seconds>( valid.time_since_epoch() ).count() },
					{ "created", duration_cast<seconds>( system_clock::now().time_since_epoch() ).count() },
					{ "token", token }
				};
			}
		};

		this->m_resources[7] = {
			"^/api/users(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::ADMIN
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				// This helper method returns a list of settings that can be used to edit a new or existing timer.
				auto fGetSettings = []( bool new_ ) -> json {
					json settings = json::array();
					settings += {
						{ "name", "name" },
						{ "label", "Name" },
						{ "type", "string" },
						{ "maxlength", 64 },
						{ "minlength", 3 },
						{ "mandatory", true },
						{ "default", "New User" },
						{ "sort", 1 }
					};
					settings += {
						{ "name", "username" },
						{ "label", "Username" },
						{ "type", "string" },
						{ "maxlength", 64 },
						{ "minlength", 3 },
						{ "mandatory", true },
						{ "sort", 2 }
					};
					if ( new_ ) {
						settings += {
							{ "name", "password" },
							{ "label", "Password" },
							{ "type", "password" },
							{ "maxlength", 64 },
							{ "minlength", 3 },
							{ "mandatory", true },
							{ "sort", 3 }
						};
					} else {
						settings += {
							{ "name", "password" },
							{ "label", "Password" },
							{ "placeholder", "Enter new password or leave blank to keep old password" },
							{ "type", "password" },
							{ "maxlength", 64 },
							{ "minlength", 3 },
							{ "sort", 3 }
						};
					}
					settings += {
						{ "name", "rights" },
						{ "label", "Rights" },
						{ "type", "list" },
						{ "mandatory", true },
						{ "options", {
							{
								{ "value", 1 },
								{ "label", "Viewer" }
							},
							{
								{ "value", 2 },
								{ "label", "User" }
							},
							{
								{ "value", 3 },
								{ "label", "Installer" }
							},
							{
								{ "value", 99 },
								{ "label", "Admin" }
							}
						} }
					};
					settings += {
						{ "name", "enabled" },
						{ "label", "Enabled" },
						{ "type", "boolean" },
						{ "mandatory", true },
						{ "default", true },
						{ "sort", 4 }
					};
					return settings;
				};

				json user = json::object();
				int userId = -1;
				auto find = input_.find( "$2" );
				if ( find != input_.end() ) {
					try {
						if (
							"settings" == jsonGet<>( *find )
							&& method_ == WebServer::Method::GET
						) {
							output_["data"] = fGetSettings( true );
							output_["code"] = 200;
							return;
						} else {
							userId = jsonGet<int>( *find );
							user = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `username`, `rights`, `enabled` "
								"FROM `users` "
								"WHERE `id`=%d ",
								userId
							);
						}
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( userId != -1 ) {
							output_["data"] = user;
							output_["data"]["settings"] = fGetSettings( false );
						} else {
							output_["data"] = g_database->getQuery<json>(
								"SELECT `id`, `name`, `username`, `rights`, `enabled` "
								"FROM `users` "
								"ORDER BY `name` ASC"
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
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						if (
							method_ == WebServer::Method::POST
							&& userId != -1
						) {
							return;
						} else if (
							method_ == WebServer::Method::PUT
							&& userId == -1
						) {
							return;
						}

						std::vector<std::string> invalid;
						std::vector<std::string> missing;
						std::vector<std::string> errors;
						json userData = json::object();
						WebServer::_validateSettings( input_, userData, fGetSettings( userId == -1 ), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							throw WebServer::ResourceException( 400, "User.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "User.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						// A password is optional for existing users and mandatory for new ones. If one has been
						// provided it needs to be hashed before it is inserted or updated in the database.
						find = input_.find( "password" );
						if ( find != input_.end() ) {
							std::string salt = randomString( 64 );
							std::string pepper = g_settings->get( WEBSERVER_SETTING_HASH_PEPPER );
							userData["password"] = this->_hash( salt + pepper + jsonGet<>( *find ) + WEBSERVER_HASH_MAGGI ) + '.' + salt;
						} else {
							// If the user was updated without providing a new password, the old password hash is added
							// to the user object so it can safely be stored again below.
							userData["password"] = g_database->getQueryValue<std::string>(
								"SELECT `password` "
								"FROM `users` "
								"WHERE `id`=%d ",
								userId
							);
						}

						if ( userId == -1 ) {
							userId = g_database->putQuery(
								"INSERT INTO `users` (`name`, `username`, `password`, `rights`, `enabled`) "
								"VALUES (%Q, %Q, %Q, %d, %d) ",
								userData["name"].get<std::string>().c_str(),
								userData["username"].get<std::string>().c_str(),
								userData["password"].get<std::string>().c_str(),
								userData["rights"].get<unsigned int>(),
								userData["enabled"].get<bool>() ? 1 : 0
							);
							output_["data"] = { "id", userId };
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `users` "
								"SET `name`=%Q, `username`=%Q, `password`=%Q, `rights`=%d, `enabled`=%d "
								"WHERE `id`=%d",
								userData["name"].get<std::string>().c_str(),
								userData["username"].get<std::string>().c_str(),
								userData["password"].get<std::string>().c_str(),
								userData["rights"].get<unsigned int>(),
								userData["enabled"].get<bool>() ? 1 : 0,
								userId
							);
							output_["code"] = 200;
						}
						break;
					}

					default: break;
				}
			}
		};

		this->m_resources[8] = {
			"^/api/user/state/([0-9a-z_]+)$",
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
			[&]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if (
					user_ == nullptr
					|| user_->getRights() < User::Rights::VIEWER
				) {
					throw WebServer::ResourceException( 403, "Access.Denied", "Access to the requested resource was denied." );
				}

				switch( method_ ) {

					case WebServer::Method::GET: {
						if (
							input_["$1"].is_string()
							&& user_->getSettings()->contains( WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX + input_["$1"].get<std::string>() )
						) {
							output_["data"] = json::parse( user_->getSettings()->get( WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX + input_["$1"].get<std::string>() ) );
							output_["code"] = 200;

						}
						break;
					}

					case WebServer::Method::PUT: {
						auto find = input_.find( "data" );
						if (
							find != input_.end()
							&& input_["$1"].is_string()
						) {
							user_->getSettings()->put( WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX + input_["$1"].get<std::string>(), (*find).dump() );
							user_->getSettings()->commit();
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::DELETE: {
						if (
							input_["$1"].is_string()
							&& user_->getSettings()->contains( WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX + input_["$1"].get<std::string>() )
						) {
							user_->getSettings()->remove( WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX + input_["$1"].get<std::string>() );
							user_->getSettings()->commit();
							output_["code"] = 200;
						}
						break;
					}
					default: break;
				}
			}
		};
	};

	bool WebServer::_validateSettings( const json& input_, json& output_, const json& settings_, std::vector<std::string>* invalid_, std::vector<std::string>* missing_, std::vector<std::string>* errors_ ) {
		bool result = true;

		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			auto setting = *settingsIt;
			const std::string& name = setting["name"].get<std::string>();
			const std::string& label = setting["label"].get<std::string>();
			const std::string& type = setting["type"].get<std::string>();

			// Search for the setting name in the input parameters. The name in the defenition should
			// me used as the key when submitting settings.
			auto find = input_.find( name );
			if (
				find != input_.end()
				&& ! (*find).is_null()
				&& (
					! (*find).is_string()
					|| (*find).get<std::string>().size() > 0
				)
			) {

				// Then the submitted value is validated depending on the type defined in the setting.
				try {
					if (
						type == "double"
						|| type == "float"
					) {

						double value = jsonGet<double>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<double>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<double>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;

					} else if ( type == "boolean" ) {

						bool value = jsonGet<bool>( *find );
						output_[name] = value;

						// If this boolean property was set additional settings might be available which also  need to
						// be  stored in the settings object.
						if (
							value
							&& ( find = setting.find( "settings" ) ) != setting.end()
						) {
							result = result && WebServer::_validateSettings( input_, output_, *find, invalid_, missing_, errors_ );
						}

					} else if ( type == "byte" ) {

						unsigned int value = jsonGet<unsigned int>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;

					} else if ( type == "short" ) {

						short value = jsonGet<short>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<int>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<int>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;

					} else if ( type == "int" ) {

						int value = jsonGet<int>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<int>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<int>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;

					} else if (
						type == "list"
						|| type == "select"
					) {

						std::string value = jsonGet<std::string>( *find );

						bool exists = false;
						bool hasSettings = false;
						json::iterator optionsIt;
						for ( optionsIt = setting["options"].begin(); optionsIt != setting["options"].end(); optionsIt++ ) {
							std::string option;
							if ( (*optionsIt)["value"].is_number() ) {
								option = std::to_string( (*optionsIt)["value"].get<int>() );
							} else if ( (*optionsIt)["value"].is_string() ) {
								option = (*optionsIt)["value"].get<std::string>();
							} else {
								continue;
							}
							if ( option == value ) {
								exists = true;
								if ( ( find = (*optionsIt).find( "settings" ) ) != (*optionsIt).end() ) {
									hasSettings = true;
								}
								break;
							}
						}
						if ( ! exists ) {
							throw std::runtime_error( "value not a valid option" );
						}
						output_[name] = (*optionsIt)["value"];

						// If a certain list option is selected that has it's own additional settings then these
						// settings need to be stored aswell.
						if ( hasSettings ) {
							result = result && WebServer::_validateSettings( input_, output_, *find, invalid_, missing_, errors_ );
						}

					} else if ( type == "multiselect" ) {

						auto values = jsonGet<std::vector<std::string>>( *find );
						for ( auto &value : values ) {
							bool exists = false;
							for ( auto &option : setting["options"] ) {
								exists = exists || ( jsonGet<>( option, "value" ) == value );
								if ( exists ) {
									break;
								}
							}
							if ( ! exists ) {
								throw std::runtime_error( "value not a valid option" );
							}
						}
						output_[name] = values;

					} else if (
						type == "string"
						|| type == "password"
					) {

						std::string value = jsonGet<std::string>( *find );
						if (
							( find = setting.find( "maxlength" ) ) != setting.end()
							&& value.size() > (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too long" );
						}
						if (
							( find = setting.find( "minlength" ) ) != setting.end()
							&& value.size() < (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too short" );
						}
						output_[name] = value;

					} else if ( type == "display" ) {
						/* nothing to validate here */
					} else {
						throw std::logic_error( "invalid type " + type );
					}

				} catch( std::runtime_error exception_ ) { // thrown by jsonGet if conversion fails
					if ( invalid_ ) invalid_->push_back( name );
					if ( errors_ ) errors_->push_back( "invalid value for " + label + " (" + exception_.what() + ")" );
					result = false;
				} catch( ... ) {
					if ( invalid_ ) invalid_->push_back( name );
					result = false;
				}

			// The setting is missing from the input. See if the setting is mandatory, which would cause a problem if
			// there's no default specified.
			} else if (
				( find = setting.find( "mandatory" ) ) != setting.end()
				&& (*find).get<bool>()
			) {

				// If there was a default specified than the field will be set to that value and no error is reported.
				if ( ( find = setting.find( "default" ) ) != setting.end() ) {
					output_[name] = *find;
				} else {
					if ( missing_ ) missing_->push_back( name );
					result = false;
				}

			// If the setting was not found and also was not mandatory, a null value is passed along. This can be used
			// to reset a value.
			} else {
				output_[name] = nullptr;
			}
		}

		return result;
	};

}; // namespace micasa
