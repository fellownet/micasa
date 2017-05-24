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

#include "hardware/Dummy.h"
#include "hardware/HarmonyHub.h"
#include "hardware/SolarEdge.h"
#include "hardware/WeatherUnderground.h"
#include "hardware/RFXCom.h"
#include "hardware/Telegram.h"
#ifdef _WITH_OPENZWAVE
	#include "hardware/ZWave.h"
#endif // _WITH_OPENZWAVE
#ifdef _WITH_LINUX_SPI
	#include "hardware/PiFace.h"
#endif // _WITH_LINUX_SPI

#include "json.hpp"

#define WEBSERVER_HASH_MAGGI "6238fbba-1f79-11e7-93ae-92361f002671"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<Settings<>> g_settings;
	
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
		m_sslport( sslport_ )
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
		for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
			Logger::logr( Logger::LogLevel::ERROR, this, "Resource %s not removed.", (*resourceIt)->uri.c_str() );
		}
		assert( this->m_resources.size() == 0 && "All resources should be removed before the global WebServer instance is destroyed." );
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

		this->_installHardwareResourceHandler();
		this->_installDeviceResourceHandler();
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

		this->m_resources.clear();

		Logger::log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::string WebServer::_hash( const std::string& data_ ) const {
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
	};

	inline void WebServer::_processRequest( std::shared_ptr<Network::Connection> connection_ ) {
		auto uri = connection_->getUri();
		if ( __unlikely( uri.substr( 0, 4 ) != "/api" ) ) {

			connection_->serve( "www" );

		} else {

			auto method = WebServer::resolveTextMethod( connection_->getMethod() );
			auto headers = connection_->getHeaders();
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
						if ( login.first > system_clock::now() ) {
							user = login.second;
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
					for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
						std::regex pattern( (*resourceIt)->uri );
						std::smatch match;
						if (
							std::regex_search( uri, match, pattern )
							&& ( (*resourceIt)->methods & method ) == method
						) {
							// Add each matched paramter to the input for the callback to determine which individual
							// resource was accessed.
							for ( unsigned int i = 1; i < match.size(); i++ ) {
								if ( match.str( i ).size() > 0 ) {
									input["$" + std::to_string( i )] = match.str( i );
								}
							}
							(*resourceIt)->callback( user, input, method, output );
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
				connection_->reply( content, code, {
					{ "Content-Type", "Content-type: application/json" },
					{ "Access-Control-Allow-Origin", "*" },
					{ "Cache-Control", "no-cache, no-store, must-revalidate" }
				} );
			} );
		}
	};

	void WebServer::_installHardwareResourceHandler() {
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/hardware(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				auto fGetSettings = []( std::shared_ptr<Hardware> hardware_ = nullptr ) {
					if ( __unlikely( hardware_ == nullptr ) ) {
						json settings = json::array();
						settings += {
							{ "name", "name" },
							{ "label", "Name" },
							{ "type", "string" },
							{ "maxlength", 64 },
							{ "minlength", 3 },
							{ "mandatory", true },
							{ "sort", 1 }
						};
						settings += {
							{ "name", "type" },
							{ "label", "Type" },
							{ "type", "list" },
							{ "options", {
								{
									{ "value", "dummy" },
									{ "label", Dummy::label }
								},
								{
									{ "value", "harmony_hub" },
									{ "label", HarmonyHub::label }
								},
#ifdef _WITH_OPENZWAVE
								{
									{ "value", "zwave" },
									{ "label", ZWave::label }
								},
#endif // _WITH_OPENZWAVE
#ifdef _WITH_LINUX_SPI
								{
									{ "value", "piface" },
									{ "label", PiFace::label }
								},
#endif // _WITH_LINUX_SPI
								{
									{ "value", "solaredge" },
									{ "label", SolarEdge::label }
								},
								{
									{ "value", "weather_underground" },
									{ "label", WeatherUnderground::label }
								},
								{
									{ "value", "rfxcom" },
									{ "label", RFXCom::label }
								},
								{
									{ "value", "telegram" },
									{ "label", Telegram::label }
								}
							} },
							{ "sort", 2 }
						};
						return settings;
					} else {
						return hardware_->getSettingsJson();
					}
				};

				std::shared_ptr<Hardware> hardware = nullptr;
				int hardwareId = -1;
				auto find = input_.find( "$2" ); // inner uri regexp match
				if ( find != input_.end() ) {
					try {
						if (
							"settings" == (*find).get<std::string>()
							&& method_ == WebServer::Method::GET
						) {
							output_["data"] = fGetSettings( nullptr );
							output_["code"] = 200;
							return;
						} else {
							hardwareId = std::stoi( (*find).get<std::string>() );
							if ( nullptr == ( hardware = g_controller->getHardwareById( hardwareId ) ) ) {
								return;
							}
						}
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {

					case WebServer::Method::GET: {
						if ( hardwareId != -1 ) {
							output_["data"] = hardware->getJson( true );
						} else {
							output_["data"] = json::array();

							std::shared_ptr<Hardware> parent = nullptr;
							try {
								parent = g_controller->getHardwareById( jsonGet<unsigned int>( input_, "hardware_id" ) );
								if ( parent == nullptr ) {
									throw WebServer::ResourceException( 400, "Hardware.Invalid.Id", "The supplied hardware id is invalid." );
								}
							} catch( std::runtime_error exception_ ) { /* ignore */ }

							auto hardwareList = g_controller->getAllHardware();
							for ( auto hardwareIt = hardwareList.begin(); hardwareIt != hardwareList.end(); hardwareIt++ ) {
								if ( (*hardwareIt)->getParent() == parent ) {
									output_["data"] += (*hardwareIt)->getJson( false );
								}
							}
						}
						output_["code"] = 200;
						break;
					}

					case WebServer::Method::DELETE: {
						if ( hardwareId != -1 ) {
							g_controller->removeHardware( hardware );
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT:
					case WebServer::Method::POST: {
						if (
							method_ == WebServer::Method::POST
							&& hardwareId != -1
						) {
							break;
						} else if (
							method_ == WebServer::Method::PUT
							&& hardwareId == -1
						) {
							break;
						}

						std::vector<std::string> invalid;
						std::vector<std::string> missing;
						std::vector<std::string> errors;
						json hardwareData = json::object();
						validateSettings( input_, hardwareData, fGetSettings( hardware ), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							//throw WebServer::ResourceException( 400, "Hardware.Invalid.Fields", "Invalid value for fields " + stringJoin( invalid, ", " ) );
							throw WebServer::ResourceException( 400, "Hardware.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Hardware.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						if ( hardwareId == -1 ) {
							Hardware::Type type = Hardware::resolveTextType( hardwareData["type"].get<std::string>() );
							std::string reference = randomString( 16 );
							hardware = g_controller->declareHardware( type, reference, { }, false ); // start disabled
							hardwareId = hardware->getId();
							output_["data"] = hardware->getJson( true );
							output_["code"] = 201; // Created
						} else {

							// The enabled property can be used to enable or disable the hardware. For now this is only
							// possible on parent/main hardware, no children.
							bool enabled = ( hardware->getState() != Hardware::State::DISABLED );
							if ( hardware->getParent() == nullptr ) {
								enabled = jsonGet<bool>( hardwareData, "enabled" );
								hardwareData.erase( "enabled" );

								g_database->putQuery(
									"UPDATE `hardware` "
									"SET `enabled`=%d "
									"WHERE `id`=%d "
									"OR `hardware_id`=%d", // also children
									enabled ? 1 : 0,
									hardware->getId(),
									hardware->getId()
								);
							}

							hardware->putSettingsJson( hardwareData );
							hardware->getSettings()->put( hardwareData );
							bool restart = false;
							if ( hardware->getSettings()->isDirty() ) {
								hardware->getSettings()->commit();
								// Only parent hardware is restarted. Child hardware needs to be restarted after an
								// update it should listen for putSettingsJon.
								if ( hardware->getParent() == nullptr ) {
									restart = true;
								}
							}

							this->m_scheduler.schedule( 0, 1, this, [hardware,enabled,restart]( std::shared_ptr<Scheduler::Task<>> ) {
								auto hardwareList = g_controller->getAllHardware();
								if (
									! enabled
									|| restart
								) {
									if ( hardware->getState() != Hardware::State::DISABLED ) {
										hardware->stop();
									}
									for ( auto hardwareIt = hardwareList.begin(); hardwareIt != hardwareList.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware
											&& (*hardwareIt)->getState() != Hardware::State::DISABLED
										) {
											(*hardwareIt)->stop();
										}
									}
								}
								if ( enabled ) {
									if ( hardware->getState() == Hardware::State::DISABLED ) {
										hardware->start();
									}
								}
							} );
							output_["code"] = 200;
						}
						break;
					}

					default: break;
				}
			} )
		) );
	};

	void WebServer::_installDeviceResourceHandler() {
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/devices(/([0-9]+))?$",
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::VIEWER ) {
					return;
				}

				std::shared_ptr<Device> device = nullptr;
				int deviceId = -1;
				auto find = input_.find( "$2" ); // inner uri regexp match
				if ( find != input_.end() ) {
					deviceId = std::stoi( (*find).get<std::string>() );
					if ( nullptr == ( device = g_controller->getDeviceById( deviceId ) ) ) {
						return;
					}
				}

				switch( method_ ) {

					case WebServer::Method::GET: {
						if ( deviceId != -1 ) {
							output_["data"] = device->getJson( user_->getRights() >= User::Rights::INSTALLER );
							if ( user_->getRights() >= User::Rights::INSTALLER ) {
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
							output_["data"] = json::array();

							// The list of devices can be filtered using various filters. Non-installers have a
							// mandatory filter on enabled.
							std::shared_ptr<Hardware> hardware = nullptr;
							bool deviceIdsFilter = false;
							std::vector<std::string> deviceIds;
							bool enabledFilter = ( user_->getRights() < User::Rights::INSTALLER );
							bool enabled = true;

							if ( user_->getRights() >= User::Rights::INSTALLER ) {
								try {
									hardware = g_controller->getHardwareById( jsonGet<unsigned int>( input_, "hardware_id" ) );
									if ( hardware == nullptr ) {
										throw WebServer::ResourceException( 400, "Hardware.Invalid.Id", "The supplied hardware id is invalid." );
									}
								} catch( std::runtime_error exception_ ) { /* ignore */ }

								try {
									deviceIds = g_database->getQueryColumn<std::string>(
										"SELECT DISTINCT `device_id` "
										"FROM `x_device_scripts` "
										"WHERE `script_id`=%d "
										"ORDER BY `device_id` ASC",
										jsonGet<unsigned int>( input_, "script_id" )
									);
									deviceIdsFilter = true;
								} catch( std::runtime_error exception_ ) { /* ignore */ }

								try {
									enabled = jsonGet<bool>( input_, "enabled" );
									enabledFilter = true;
								} catch( std::runtime_error exception_ ) { /* ignore */ }
							}

							try {
								auto additionalDeviceIds = stringSplit( jsonGet<std::string>( input_, "device_ids" ), ',' );
								deviceIds.insert( deviceIds.end(), additionalDeviceIds.begin(), additionalDeviceIds.end() );
								deviceIdsFilter = true;
							} catch( std::runtime_error exception_ ) { /* ignore */ }
							
							auto devices = g_controller->getAllDevices();
							for ( auto deviceIt = devices.begin(); deviceIt != devices.end(); deviceIt++ ) {
								if ( enabledFilter ) {
									if ( enabled && ! (*deviceIt)->isEnabled() ) {
										continue;
									}
									if ( ! enabled && (*deviceIt)->isEnabled() ) {
										continue;
									}
								}
								if ( deviceIdsFilter ) {
									if ( std::find( deviceIds.begin(), deviceIds.end(), std::to_string( (*deviceIt)->getId() ) ) == deviceIds.end() ) {
										continue;
									}
								}
								if (
									hardware == nullptr
									|| (*deviceIt)->getHardware() == hardware
								) {
									output_["data"] += (*deviceIt)->getJson( false );
								}
							}
						}
						output_["code"] = 200;
						break;
					}

					case WebServer::Method::DELETE: {
						if (
							user_->getRights() >= User::Rights::INSTALLER
							&& deviceId != -1
						) {
							device->getHardware()->removeDevice( device );
							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PUT: {
						if (
							user_->getRights() >= User::Rights::INSTALLER
							&& deviceId != -1
						) {
							std::vector<std::string> invalid;
							std::vector<std::string> missing;
							std::vector<std::string> errors;
							json deviceData = json::object();
							validateSettings( input_, deviceData, device->getSettingsJson(), &invalid, &missing, &errors );
							if ( invalid.size() > 0 ) {
								//throw WebServer::ResourceException( 400, "Device.Invalid.Fields", "Invalid value for fields " + stringJoin( invalid, ", " ) );
								throw WebServer::ResourceException( 400, "Device.Invalid.Fields", stringJoin( errors, ", " ) );
							} else if ( missing.size() > 0 ) {
								throw WebServer::ResourceException( 400, "Device.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
							}

							bool enabled = jsonGet<bool>( deviceData, "enabled" );
							deviceData.erase( "enabled" ); // prevents it from ending up in settings
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

							device->putSettingsJson( deviceData );
							device->getSettings()->put( deviceData );
							if ( device->getSettings()->isDirty() ) {
								device->getSettings()->commit();
							}

							// A scripts array can be passed along to set the scripts to run when the device is updated.
							auto find = input_.find( "scripts");
							if ( find != input_.end() ) {
								if ( (*find).is_array() ) {
									std::vector<unsigned int> scripts = std::vector<unsigned int>( (*find).begin(), (*find).end() );
									device->setScripts( scripts );
								} else {
									throw WebServer::ResourceException( 400, "Device.Invalid.Scripts", "The supplied scripts parameter is invalid." );
								}
							}

							output_["code"] = 200;
						}
						break;
					}

					case WebServer::Method::PATCH: {
						if (
							deviceId != -1
							&& ( device->getSettings()->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & Device::UpdateSource::API ) == Device::UpdateSource::API
							&& user_->getRights() >= device->getSettings()->get<User::Rights>( DEVICE_SETTING_MINIMUM_USER_RIGHTS, User::Rights::USER )
						) {
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
										device->updateValue<Switch>( Device::UpdateSource::API, jsonGet<std::string>( input_, "value" ) );
										break;
								}
							} catch( std::runtime_error exception_ ) {
								throw WebServer::ResourceException( 400, "Device.Invalid.Value", "The supplied value is invalid." );
							}

							output_["code"] = 200;
						}
						break;
					}

					default: break;
				}

			} )
		) );

		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/devices/([0-9]+)/data$",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::VIEWER ) {
					return;
				}
				
				std::shared_ptr<Device> device = nullptr;
				int deviceId = -1;
				auto find = input_.find( "$1" ); // inner uri regexp match
				if ( find != input_.end() ) {
					deviceId = std::stoi( (*find).get<std::string>() );
					if ( nullptr == ( device = g_controller->getDeviceById( deviceId ) ) ) {
						return;
					}
				}
			
				try {
					switch( device->getType() ) {
						case Device::Type::SWITCH:
							output_["data"] = std::static_pointer_cast<Switch>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<std::string>( input_, "interval" ) );
							break;
						case Device::Type::COUNTER:
							output_["data"] = std::static_pointer_cast<Counter>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<std::string>( input_, "interval" ), jsonGet<std::string>( input_, "group" ) );
							break;
						case Device::Type::LEVEL:
							output_["data"] = std::static_pointer_cast<Level>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<std::string>( input_, "interval" ), jsonGet<std::string>( input_, "group" ) );
							break;
						case Device::Type::TEXT:
							output_["data"] = std::static_pointer_cast<Text>( device )
								->getData( jsonGet<unsigned int>( input_, "range" ), jsonGet<std::string>( input_, "interval" ) );
							break;
					}
				} catch( ... ) {
					throw WebServer::ResourceException( 400, "Device.Invalid.Parameters", "The supplied parameters are invalid." );
				}
			
				output_["code"] = 200;
			} )
		) );
		
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/devices/([0-9]+)/links(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::POST | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				// A device id is mandatory for the url to be matched, so it can be retrieved unconditionally.
				unsigned int deviceId = jsonGet<unsigned int>( input_, "$1" );
				std::shared_ptr<Device> device = g_controller->getDeviceById( deviceId );
				if ( device == nullptr ) {
					return;
				}

				// This helper method returns a list of settings that can be used to edit a new or existing link.
				auto fGetSettings = [device]( bool new_ ) -> json {
					auto devices  = g_controller->getAllDevices();
					json compatibleDevices = json::array();
					for ( auto devicesIt = devices.begin(); devicesIt != devices.end(); devicesIt++ ) {
						auto updateSources = (*devicesIt)->getSettings()->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES );
						bool readOnly = ( Device::resolveUpdateSource( updateSources & Device::UpdateSource::USER ) == 0 );
						if (
							(
								(*devicesIt)->getType() == device->getType()
								|| ( (*devicesIt)->getType() == Device::Type::LEVEL && device->getType() == Device::Type::COUNTER )
								|| ( (*devicesIt)->getType() == Device::Type::COUNTER && device->getType() == Device::Type::LEVEL )
							)
							&& (*devicesIt)->isEnabled()
							&& (
								device->getType() != Device::Type::SWITCH
								|| ! readOnly
							)
							&& (*devicesIt) != device
						) {
							compatibleDevices += {
								{ "value", (*devicesIt)->getId() },
								{ "label", (*devicesIt)->getName() }
							};
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
						{ "default", true },
						{ "sort", 2 }
					};
					settings += {
						{ "name", "device" },
						{ "label", "Source Device" },
						{ "type", "display" },
						{ "value", device->getName() },
						{ "sort", 10 }
					};
					settings += {
						{ "name", "target_device_id" },
						{ "label", "Target Device" },
						{ "type", "list" },
						{ "mandatory", true },
						{ "options", compatibleDevices },
						{ "sort", 11 }
					};

					if ( device->getType() == Device::Type::SWITCH ) {
						settings += {
							{ "name", "value" },
							{ "label", "Value" },
							{ "description", "The value that triggers this link." },
							{ "default", "On" },
							{ "type", "list" },
							{ "options", switchOptions },
							{ "sort", 12 }
						};
						settings += {
							{ "name", "target_value" },
							{ "label", "Target Value" },
							{ "description", "The value to set the target device to when the link is triggered." },
							{ "default", "On" },
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
					}

					return settings;
				};

				// A link identifier is optional, but if it exist it is guarantieed to be of type string.
				json link = json::object();
				int linkId = -1;
				auto find = input_.find( "$3" ); // inner uri regexp match
				if ( find != input_.end() ) {
					try {
						if (
							"settings" == (*find).get<std::string>()
							&& method_ == WebServer::Method::GET
						) {
							output_["data"] = fGetSettings( true );
							output_["code"] = 200;
							return;
						} else {
							linkId = std::stoi( (*find).get<std::string>() );
							link = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `device_id`, `target_device_id`, `value`, `target_value`, `after`, `for`, `clear`, `enabled` "
								"FROM `links` "
								"WHERE `id`=%d "
								"AND `device_id`=%d",
								linkId,
								deviceId
							);
						}
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {

					case WebServer::Method::GET: {
						if ( linkId != -1 ) {
							// The detailed fetch of a link contains actual device data.
							output_["data"] = link;
							output_["data"]["device"] = g_controller->getDeviceById( link["device_id"].get<unsigned int>() )->getJson();
							output_["data"]["target_device"] = g_controller->getDeviceById( link["target_device_id"].get<unsigned int>() )->getJson();
							output_["data"]["settings"] = fGetSettings( false );
						} else {
							// The overview list of links contain only names for the devices.
							output_["data"] = g_database->getQuery<json>(
								"SELECT `id`, `name`, `device_id`, `target_device_id`, `value`, `target_value`, `after`, `for`, `clear`, `enabled` "
								"FROM `links` "
								"WHERE `device_id`=%d "
								"OR `target_device_id`=%d "
								"ORDER BY `created`",
								deviceId,
								deviceId
							);
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
						validateSettings( input_, linkData, fGetSettings( linkId == -1 ), &invalid, &missing, &errors );
						if ( invalid.size() > 0 ) {
							//throw WebServer::ResourceException( 400, "Link.Invalid.Fields", "Invalid value for fields " + stringJoin( invalid, ", " ) );
							throw WebServer::ResourceException( 400, "Link.Invalid.Fields", stringJoin( errors, ", " ) );
						} else if ( missing.size() > 0 ) {
							throw WebServer::ResourceException( 400, "Link.Missing.Fields", "Missing value for fields " + stringJoin( missing, ", " ) );
						}

						if ( linkId == -1 ) {
							linkId = g_database->putQuery(
								"INSERT INTO `links` (`name`, `device_id`, `target_device_id`, `enabled`, `value`, `target_value`, `after`, `for`, `clear`) "
								"VALUES (%Q, %d, %d, %d, %Q, %Q, %Q, %Q, %d) ",
								linkData["name"].get<std::string>().c_str(),
								deviceId,
								linkData["target_device_id"].get<unsigned int>(),
								linkData["enabled"].get<bool>() ? 1 : 0,
								linkData["value"].is_null() ? NULL : linkData["value"].get<std::string>().c_str(),
								linkData["target_value"].is_null() ? NULL : linkData["target_value"].get<std::string>().c_str(),
								linkData["after"].is_null() ? NULL : std::to_string( linkData["after"].get<double>() ).c_str(),
								linkData["for"].is_null() ? NULL : std::to_string( linkData["for"].get<double>() ).c_str(),
								linkData["clear"].is_null() ? 0 : ( linkData["clear"].get<bool>() ? 1 : 0 )
							);
							link["id"] = linkId;
							output_["data"] = link;
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `links` "
								"SET `name`=%Q, `device_id`=%d, `target_device_id`=%d, `enabled`=%d, `value`=%Q, `target_value`=%Q, `after`=%Q, `for`=%Q, `clear`=%d "
								"WHERE `id`=%d",
								linkData["name"].get<std::string>().c_str(),
								deviceId,
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
			} )
		) );
	};

	void WebServer::_installScriptResourceHandler() {
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/scripts(/([0-9]+))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				json script = json::object();
				int scriptId = -1;
				auto find = input_.find( "$2" ); // inner uri regexp match
				if ( find != input_.end() ) {
					try {
						scriptId = std::stoi( (*find).get<std::string>() );
						script = g_database->getQueryRow<json>(
							"SELECT `id`, `name`, `code`, `enabled` "
							"FROM `scripts` "
							"WHERE `id`=%d ",
							scriptId
						);
					} catch( ... ) {
						return;
					}
				}
				
				switch( method_ ) {

					case WebServer::Method::GET: {
						if ( scriptId != -1 ) {
							output_["data"] = script;
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
					
						auto find = input_.find( "name" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								script["name"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( 400, "Script.Invalid.Name", "The supplied name is invalid." );
							}
						} else if ( scriptId == -1 ) {
							throw WebServer::ResourceException( 400, "Script.Missing.Name", "Missing name." );
						}

						find = input_.find( "code" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								script["code"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( 400, "Script.Invalid.Code", "The supplied code is invalid." );
							}
						} else if ( scriptId == -1 ) {
							throw WebServer::ResourceException( 400, "Script.Missing.Code", "Missing code." );
						}

						find = input_.find( "enabled" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								script["enabled"] = ( (*find).get<std::string>() == "1" || (*find).get<std::string>() == "true" || (*find).get<std::string>() == "yes" );
							} else if ( (*find).is_number() ) {
								script["enabled"] = (*find).get<unsigned short>() > 0;
							} else if ( (*find).is_boolean() ) {
								script["enabled"] = (*find).get<bool>();
							} else {
								throw WebServer::ResourceException( 400, "Script.Invalid.Enabled", "The supplied enabled parameter is invalid." );
							}
						} else if ( scriptId == -1 ) {
							script["enabled"] = true;
						}

						if ( scriptId == -1 ) {
							scriptId = g_database->putQuery(
								"INSERT INTO `scripts` (`name`, `code`, `enabled`) "
								"VALUES (%Q, %Q, %d) ",
								script["name"].get<std::string>().c_str(),
								script["code"].get<std::string>().c_str(),
								script["enabled"].get<bool>() ? 1 : 0
							);
							script["id"] = scriptId;
							output_["data"] = script;
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `scripts` "
								"SET `name`=%Q, `code`=%Q, `enabled`=%d "
								"WHERE `id`=%d",
								script["name"].get<std::string>().c_str(),
								script["code"].get<std::string>().c_str(),
								script["enabled"].get<bool>() ? 1 : 0,
								scriptId
							);
							output_["code"] = 200;
						}
						output_["data"] = script;
						break;
					}
					
					default: break;
				}
			} )
		) );
	};

	void WebServer::_installTimerResourceHandler() {
		// https://www.liberiangeek.net/2013/05/is-there-a-better-task-scheduler-in-ubuntu-13-04-raring-ringtail/		
		// https://www.liberiangeek.net/wp-content/uploads/2013/05/gnome_schedule_ubuntu1304_1_thumb.png
		// https://www.ghacks.net/2011/03/09/schedule-cron-jobs-with-this-easy-to-use-gui/
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/timers(/([0-9]+|new))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				json timer = json::object();
				int timerId = -1;
				auto find = input_.find( "$2" ); // inner uri regexp match
				if ( find != input_.end() ) {
					try {
						timerId = std::stoi( (*find).get<std::string>() );
						timer = g_database->getQueryRow<json>(
							"SELECT `id`, `name`, `cron`, `enabled` "
							"FROM `timers` "
							"WHERE `id`=%d ",
							timerId
						);
					} catch( ... ) {
						return;
					}
				}

				switch( method_ ) {

					case WebServer::Method::GET: {
						if ( timerId != -1 ) {
							output_["data"] = timer;
							auto find = input_.find( "device_id" );
							if ( find != input_.end() ) {
								unsigned int deviceId;
								if ( (*find).is_string() ) {
									deviceId = std::stoi( (*find).get<std::string>() );
								} else if ( (*find).is_number() ) {
									deviceId = (*find).get<unsigned int>();
								} else {
									throw WebServer::ResourceException( 400, "Timer.Invalid.Device", "The supplied device id is invalid." );
								}
								output_["data"]["value"] = g_database->getQueryValue<std::string>(
									"SELECT `value` "
									"FROM `x_timer_devices` "
									"WHERE `timer_id`=%d "
									"AND `device_id`=%d",
									timerId,
									deviceId
								);
							} else {
								// Stand-alone timers can be associated with scripts. Provide a list of already associated
								// scripts for these timers on the edit page.
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
							// If a device id was provided only those timers that are set for the device should be returned.
							auto find = input_.find( "device_id" );
							if ( find != input_.end() ) {
								unsigned int deviceId;
								if ( (*find).is_string() ) {
									deviceId = std::stoi( (*find).get<std::string>() );
								} else if ( (*find).is_number() ) {
									deviceId = (*find).get<unsigned int>();
								} else {
									throw WebServer::ResourceException( 400, "Timer.Invalid.Device", "The supplied device id is invalid." );
								}
								output_["data"] = g_database->getQuery<json>(
									"SELECT t.`id`, t.`name`, t.`cron`, t.`enabled`, x.`value` "
									"FROM `timers` t, `x_timer_devices` x "
									"WHERE t.`id`=x.`timer_id` "
									"AND x.`device_id`=%d "
									"ORDER BY t.`id` ASC",
									deviceId
								);
							} else {
								// No device id was provided, so only the timers that are not associated with any devices are
								// returned.
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

						auto find = input_.find( "name" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								timer["name"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( 400, "Timer.Invalid.Name", "The supplied name is invalid." );
							}
						} else if ( timerId == -1 ) {
							throw WebServer::ResourceException( 400, "Timer.Missing.Name", "Missing name." );
						}

						find = input_.find( "cron" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								timer["cron"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( 400, "Timer.Invalid.Cron", "The supplied cron is invalid." );
							}
						} else if ( timerId == -1 ) {
							throw WebServer::ResourceException( 400, "Timer.Missing.Cron", "Missing cron." );
						}

						find = input_.find( "enabled" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								timer["enabled"] = ( (*find).get<std::string>() == "1" || (*find).get<std::string>() == "true" || (*find).get<std::string>() == "yes" );
							} else if ( (*find).is_number() ) {
								timer["enabled"] = (*find).get<unsigned short>() > 0;
							} else if ( (*find).is_boolean() ) {
								timer["enabled"] = (*find).get<bool>();
							} else {
								throw WebServer::ResourceException( 400, "Timer.Invalid.Enabled", "The supplied enabled parameter is invalid." );
							}
						} else if ( timerId == -1 ) {
							timer["enabled"] = true;
						}

						if ( timerId == -1 ) {
							timerId = g_database->putQuery(
								"INSERT INTO `timers` (`name`, `cron`, `enabled`) "
								"VALUES (%Q, %Q, %d) ",
								timer["name"].get<std::string>().c_str(),
								timer["cron"].get<std::string>().c_str(),
								timer["enabled"].get<bool>() ? 1 : 0
							);
							timer["id"] = timerId;
							output_["data"] = timer;
							output_["code"] = 201; // Created
						} else {
							g_database->putQuery(
								"UPDATE `timers` "
								"SET `name`=%Q, `cron`=%Q, `enabled`=%d "
								"WHERE `id`=%d",
								timer["name"].get<std::string>().c_str(),
								timer["cron"].get<std::string>().c_str(),
								timer["enabled"].get<bool>() ? 1 : 0,
								timerId
							);
							output_["code"] = 200;
						}

						find = input_.find( "scripts");
						if ( find != input_.end() ) {
							if ( (*find).is_array() ) {
								std::stringstream list;
								unsigned int index = 0;
								for ( auto scriptIdsIt = (*find).begin(); scriptIdsIt != (*find).end(); scriptIdsIt++ ) {
									auto scriptId = (*scriptIdsIt);
									if ( scriptId.is_number() ) {
										list << ( index > 0 ? "," : "" ) << scriptId;
										index++;
										g_database->putQuery(
											"INSERT OR IGNORE INTO `x_timer_scripts` "
											"(`timer_id`, `script_id`) "
											"VALUES (%d, %d)",
											timerId,
											scriptId.get<unsigned int>()
										);
									}
								}
								g_database->putQuery(
									"DELETE FROM `x_timer_scripts` "
									"WHERE `timer_id`=%d "
									"AND `script_id` NOT IN (%q)",
									timerId,
									list.str().c_str()
								);
							} else {
								throw WebServer::ResourceException( 400, "Timer.Invalid.Scripts", "The supplied scripts parameter is invalid." );
							}
						}
						
						find = input_.find( "device_id");
						if ( find != input_.end() ) {
							unsigned int deviceId;
							if ( (*find).is_string() ) {
								deviceId = std::stoi( (*find).get<std::string>() );
							} else if ( (*find).is_number() ) {
								deviceId = (*find).get<unsigned int>();
							} else {
								throw WebServer::ResourceException( 400, "Timer.Invalid.Device", "The supplied device id is invalid." );
							}
							auto device = g_controller->getDeviceById( deviceId );
							if ( device == nullptr ) {
								throw WebServer::ResourceException( 400, "Timer.Invalid.Device", "The supplied device id is invalid." );
							}
							
							// Validate the supplied value. NOTE the timer/device cross table stores all types of values
							// as a string.
							find = input_.find( "value");
							if ( find != input_.end() ) {
								std::string value;
								if ( (*find).is_string() ) {
									value = (*find).get<std::string>();
								} else if ( (*find).is_number() ) {
									value = std::to_string( (*find).get<unsigned int>() );
								} else {
									throw WebServer::ResourceException( 400, "Timer.Invalid.Value", "The supplied value is invalid." );
								}
								g_database->putQuery(
									"REPLACE INTO `x_timer_devices` "
									"(`timer_id`, `device_id`, `value`) "
									"VALUES (%d, %d, %Q)",
									timerId,
									deviceId,
									value.c_str()
								);
							}
						}

						output_["data"] = timer;
					}
					
					default: break;
				}
			} )
		) );
	};


	void WebServer::_installUserResourceHandler() {
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/user/(login|refresh)$",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				std::lock_guard<std::mutex> loginsLock( this->m_loginsMutex );

				auto now = system_clock::now();
				for ( auto loginsIt = this->m_logins.begin(); loginsIt != this->m_logins.end(); ) {
					if ( (*loginsIt).second.first < now ) {
						loginsIt = this->m_logins.erase( loginsIt );
					} else {
						loginsIt++;
					}
				}

				if (
					method_ == WebServer::Method::POST
					&& input_["$1"].get<std::string>() == "login"
				) {
					// The _processHttpRequest has already checked the username and password, so if no user pas passed
					// to this callback it means that either the username or password was invalid.
					if ( user_ == nullptr ) {
						Logger::log( Logger::LogLevel::WARNING, this, "Invalid username and/or password supplied." );
						throw WebServer::ResourceException( 400, "Login.Failure", "The username and/or password is invalid." );
					}
					output_["code"] = 201; // Created
					Logger::logr( Logger::LogLevel::NORMAL, this, "User %s logged in.", user_->getName().c_str() );
				} else if (
					method_ == WebServer::Method::GET
					&& input_["$1"].get<std::string>() == "refresh"
					&& user_ != nullptr
				) {
					this->m_logins.erase( jsonGet<std::string>( input_, "_token" ) );
					output_["code"] = 200; // Refreshed
					Logger::logr( Logger::LogLevel::NORMAL, this, "User %s prolonged login.", user_->getName().c_str() );
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
			} )
		) );

		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/users(/([0-9]+))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::ADMIN ) {
					return;
				}

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
						return;
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

						auto find = input_.find( "name" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								user["name"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( 400, "User.Invalid.Name", "The supplied name is invalid." );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( 400, "User.Missing.Name", "Missing name." );
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
								throw WebServer::ResourceException( 400, "User.Invalid.Username", "The supplied username is invalid." );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( 400, "User.Missing.Username", "Missing username." );
						}

						find = input_.find( "password" );
						if ( find != input_.end() ) {
							if (
								(*find).is_string()
								&& (*find).get<std::string>().size() > 2
								&& (*find).get<std::string>().size() <= 32
							) {
								user["password"] = (*find).get<std::string>();
							} else {
								throw WebServer::ResourceException( 400, "User.Invalid.Password", "The supplied password is invalid." );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( 400, "User.Missing.Password", "Missing password." );
						}
						
						find = input_.find( "rights" );
						if ( find != input_.end() ) {
							if ( (*find).is_string() ) {
								user["rights"] = std::stoi( (*find).get<std::string>() );
							} else if ( (*find).is_number() ) {
								user["rights"] = (*find).get<unsigned short>();
							} else {
								throw WebServer::ResourceException( 400, "User.Invalid.Rights", "The supplied rights are invalid." );
							}
						} else if ( userId == -1 ) {
							throw WebServer::ResourceException( 400, "User.Missing.Rights", "Missing rights." );
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
								throw WebServer::ResourceException( 400, "User.Invalid.Enabled", "The supplied enabled parameter is invalid." );
							}
						} else if ( userId == -1 ) {
							user["enabled"] = true;
						}
						
						std::string salt = randomString( 64 );
						std::string pepper = g_settings->get( WEBSERVER_SETTING_HASH_PEPPER );
						std::string password = this->_hash( salt + pepper + user["password"].get<std::string>() + WEBSERVER_HASH_MAGGI ) + '.' + salt;
						if ( userId == -1 ) {
							userId = g_database->putQuery(
								"INSERT INTO `users` (`name`, `username`, `password`, `rights`, `enabled`) "
								"VALUES (%Q, %Q, %Q, %d, %d) ",
								user["name"].get<std::string>().c_str(),
								user["username"].get<std::string>().c_str(),
								password.c_str(),
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
								password.c_str(),
								user["rights"].get<unsigned int>(),
								user["enabled"].get<bool>() ? 1 : 0,
								userId
							);
							output_["code"] = 200;
						}
						user.erase( "password" );
						output_["data"] = user;
						break;
					}

					default: break;
				}
			} )
		) );
		
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^/api/user/state/([0-9a-z_]+)$",
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::VIEWER ) {
					return;
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
			} )
		) );
	};

}; // namespace micasa
