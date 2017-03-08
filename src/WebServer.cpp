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

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "WebServer.h"

#include "Network.h"
#include "Controller.h"
#include "Database.h"
#include "Logger.h"
#include "Settings.h"
#include "User.h"

#include "hardware/Dummy.h"
#include "hardware/HarmonyHub.h"
#include "hardware/ZWave.h"
#include "hardware/SolarEdge.h"
#include "hardware/WeatherUnderground.h"
#include "hardware/RFXCom.h"
#include "hardware/P1Meter.h"
#include "hardware/Telegram.h"
#include "hardware/Sunriset.h"

#include "json.hpp"

#define KEY_FILE "./var/key.pem"
#define CERT_FILE "./var/cert.pem"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Network> g_network;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<Settings<> > g_settings;
	
	using namespace nlohmann;
	
	WebServer::WebServer() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be created before global WebServer instance." );
		assert( g_network && "Global Network instance should be created before global WebServer instance." );
		assert( g_database && "Global Database instance should be created before global WebServer instance." );
		assert( g_controller && "Global Controller instance should be created before global WebServer instance." );
#endif // _DEBUG
		srand ( time( NULL ) );
	};

	WebServer::~WebServer() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be destroyed after global WebServer instance." );
		assert( g_network && "Global Network instance should be destroyed after global WebServer instance." );
		assert( g_database && "Global Database instance should be destroyed after global WebServer instance." );
		assert( g_controller && "Global Controller instance should be destroyed after global WebServer instance." );
		for ( auto resourceIt = this->m_resources.begin(); resourceIt != this->m_resources.end(); resourceIt++ ) {
			g_logger->logr( Logger::LogLevel::ERROR, this, "Resource %s not removed.", (*resourceIt)->uri.c_str() );
		}
		assert( this->m_resources.size() == 0 && "All resources should be removed before the global WebServer instance is destroyed." );
#endif // _DEBUG
	};

	void WebServer::start() {
#ifdef _DEBUG
		assert( g_network->isRunning() && "Network should be running before WebServer is started." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		// Micasa only runs over HTTPS which requires a keys and certificate file. These are read and/or stored in the
		// userdata folder. Webtokens / authentication is also encrypted using the same keys.
		// http://stackoverflow.com/questions/256405/programmatically-create-x509-certificate-using-openssl

		if (
			access( KEY_FILE, F_OK ) != 0 
			|| access( CERT_FILE, F_OK ) != 0
		) {
			g_logger->log( Logger::LogLevel::NORMAL, this, "Generating SSL key and certificate." );

			this->m_key = EVP_PKEY_new();
			RSA* rsa = RSA_generate_key(
				2048,   // number of bits for the key
				RSA_F4, // exponent
				NULL,   // callback - can be NULL if we aren't displaying progress
				NULL    // callback argument - not needed in this case
			);
			if ( ! rsa ) {
				throw std::runtime_error( "unable to generate 2048-bit RSA key" );
			}
			EVP_PKEY_assign_RSA( this->m_key, rsa ); // now rsa will be free'd alongside m_key

			this->m_certificate = X509_new();
			ASN1_INTEGER_set( X509_get_serialNumber( this->m_certificate ), 1 ); // some webservers reject serial number 0
			X509_gmtime_adj( X509_get_notBefore( this->m_certificate ), 0 ); // not usable before 'now'
			X509_gmtime_adj( X509_get_notAfter( this->m_certificate ), (long)( 60 * 60 * 24 * 365 ) ); // not usable after xx days
			X509_set_pubkey( this->m_certificate, this->m_key );

			X509_NAME* name = X509_get_subject_name( this->m_certificate );
			X509_NAME_add_entry_by_txt( name, "C", MBSTRING_ASC, (unsigned char*)"NL", -1, -1, 0 );
			X509_NAME_add_entry_by_txt( name, "CN", MBSTRING_ASC, (unsigned char*)"Micasa", -1, -1, 0 );
			X509_NAME_add_entry_by_txt( name, "O", MBSTRING_ASC, (unsigned char*)"Fellownet", -1, -1, 0 );
			X509_set_issuer_name( this->m_certificate, name );

			// Self-sign the certificate with the private key (NOTE: when using real certificates, a CA should sign the
			// certificate). So the certificate contains the public key and is signed with the private key. This makes
			// sense for self-signed certificates.
			X509_sign( this->m_certificate, this->m_key, EVP_sha1() );

			// The private key and the certificate are written to disk in PEM format.
			FILE* f = fopen( KEY_FILE, "wb");
			PEM_write_PrivateKey(
				f,            // write the key to the file we've opened
				this->m_key,  // our key from earlier
				NULL,         // default cipher for encrypting the key on disk (EVP_des_ede3_cbc())
				NULL,         // passphrase required for decrypting the key on disk ("replace_me")
				0,            // length of the passphrase string (10)
				NULL,         // callback for requesting a password
				NULL          // data to pass to the callback
			);
			fclose( f );

			f = fopen( CERT_FILE, "wb" );
			PEM_write_X509(
				f,                  // write the certificate to the file we've opened
				this->m_certificate // our certificate
			);
			fclose( f );

		} else {

			g_logger->log( Logger::LogLevel::NORMAL, this, "Reading SSL key and certificate." );

			// The certificate contains the public key we need to encrypt data. The private key is read from the key
			// file and is also assigned to the EVP object.
			FILE* f = fopen( CERT_FILE, "r" );
			this->m_certificate = PEM_read_X509( f, NULL, NULL, NULL );
			if ( ! this->m_certificate ) {
				throw std::runtime_error( "unable to read certificate file" );
			}
			fclose( f );

			f = fopen( KEY_FILE, "r" );
			this->m_key = PEM_read_PrivateKey( f, NULL, NULL, NULL );
			if ( ! this->m_key ) {
				throw std::runtime_error( "unable to read key file" );
			}
			fclose( f );
		}

		if ( NULL == g_network->bind( "80", Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
				if ( event_ == MG_EV_HTTP_REQUEST ) {
					this->_processHttpRequest( connection_, (http_message*)data_ );
				}
			} ) )
		) {
			throw std::runtime_error( "unable to bind to port 80" );
		}

		if ( NULL == g_network->bind( "443", CERT_FILE, KEY_FILE, Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
				if ( event_ == MG_EV_HTTP_REQUEST ) {
					this->_processHttpRequest( connection_, (http_message*)data_ );
				}
			} ) )
		) {
			throw std::runtime_error( "unable to bind to port 443" );
		}

		// If there are no users defined in the database, a default administrator is created.
		if ( g_database->getQueryValue<unsigned int>( "SELECT COUNT(*) FROM `users`" ) == 0 ) {
			g_database->putQuery(
				"INSERT INTO `users` (`name`, `username`, `password`, `rights`) "
				"VALUES ( 'Administrator', 'admin', '%s', %d )",
				this->_hash( "admin" ).c_str(),
				User::resolveRights( User::Rights::ADMIN )
			);
			g_logger->log( Logger::LogLevel::NORMAL, this, "Default administrator user created." );
		}

		this->_installHardwareResourceHandler();
		this->_installDeviceResourceHandler();
		this->_installScriptResourceHandler();
		this->_installTimerResourceHandler();
		this->_installUserResourceHandler();
		
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};
	
	void WebServer::stop() {
#ifdef _DEBUG
		assert( g_network->isRunning() && "Network should be running before WebServer is stopped." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		EVP_PKEY_free( this->m_key );
		X509_free( this->m_certificate );

		{
			std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
			this->m_resources.clear();
		}

		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::string WebServer::_encrypt64( const std::string& data_ ) const {
#ifdef _DEBUG
		// The maximum length of the data to be encrypted depends on the key length of the RSA key. If more data needs
		// to be encrypted, AES-like solutions should be implemented.
		assert( data_.size() < 245 && "Maximum length of string to encrypt using RSA is ~245 bytes." );
#endif // _DEBUG

		EVP_PKEY* key = X509_get_pubkey( this->m_certificate );
		if ( ! key ) {
			throw std::runtime_error( "unable to extract public key from certificate" );
		}
		int size = EVP_PKEY_size( key );
		char encrypted[size];
		if ( ! RSA_public_encrypt( data_.size(), (unsigned char*)data_.c_str(), (unsigned char*)encrypted, EVP_PKEY_get1_RSA( key ), RSA_PKCS1_PADDING ) ) {
			throw std::runtime_error( "unable to encrypt string" );
		}
		EVP_PKEY_free( key );

		// Then the data is base64 encoded.
		BIO *buff, *b64f;
		b64f = BIO_new( BIO_f_base64() );
		buff = BIO_new( BIO_s_mem() );
		buff = BIO_push( b64f, buff );

		BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
		if ( ! BIO_set_close( buff, BIO_CLOSE ) ) {
			throw std::runtime_error( "bio set close failure" );
		}
		BIO_write(buff, encrypted, size );
		if ( ! BIO_flush( buff ) ) {
			throw std::runtime_error( "bio flush failure" );
		}

		BUF_MEM *ptr;
		BIO_get_mem_ptr( buff, &ptr );
		std::string result;
		result.assign( ptr->data, ptr->length );

		BIO_free_all( buff );

		return result;
	};
	
	std::string WebServer::_decrypt64( const std::string& data_ ) const {

		// First the data is base64 decoded.
		char decoded[data_.size()];
		BIO *buff, *b64f;
		b64f = BIO_new( BIO_f_base64() );
		buff = BIO_new_mem_buf( (void *)data_.c_str(), data_.size() );
		buff = BIO_push( b64f, buff );
		
		BIO_set_flags( buff, BIO_FLAGS_BASE64_NO_NL );
		if ( ! BIO_set_close( buff, BIO_CLOSE ) ) {
			throw std::runtime_error( "bio set close failure" );
		}
		int size1 = BIO_read( buff, decoded, data_.size() );
		BIO_free_all( buff );

		// Then the data is decrypted.
		int size2 = EVP_PKEY_size( this->m_key );
		char decrypted[size2];
		size2 = RSA_private_decrypt( size1, (unsigned char*)decoded, (unsigned char*)decrypted, EVP_PKEY_get1_RSA( this->m_key ), RSA_PKCS1_PADDING );
		if ( ! size2 ) {
			throw std::runtime_error( "rsa decrypt failure" );
		}
		std::string result( decrypted, size2 );
		return result;
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
			std::shared_ptr<User> user = nullptr;
			if ( headers.find( "Authorization" ) != headers.end() ) {
				try {
					json token = json::parse( this->_decrypt64( headers["Authorization"] ) );
					unsigned int userId = token["user_id"].get<unsigned int>();
					auto userData = g_database->getQueryRow(
						"SELECT u.`rights`, u.`name`, strftime('%%s') AS now "
						"FROM `users` u "
						"WHERE `id`=%d ",
						userId
					);
					if ( token["valid"].get<int>() >= std::stoi( userData["now"] ) ) {
						user = std::make_shared<User>( userId, userData["name"], User::resolveRights( std::stoi( userData["rights"] ) ) );
					}
				} catch( ... ) { }
			} else if (
				input.find( "username" ) != input.end()
				&& input.find( "password" ) != input.end()
			) {
				try {
					auto userData = g_database->getQueryRow(
						"SELECT `id`, `name`, `rights` "
						"FROM `users` "
						"WHERE `username`='%s' "
						"AND `password`='%s' "
						"AND `enabled`=1",
						input["username"].get<std::string>().c_str(),
						this->_hash( input["password"].get<std::string>() ).c_str()
					);
					user = std::make_shared<User>( std::stoi( userData["id"] ), userData["name"], User::resolveRights( std::stoi( userData["rights"] ) ) );
				} catch( ... ) { }
			}

			// Prepare the output json object that is eventually send back to the client. Each API request
			// results in a standardized response.
			json output = {
				{ "result", "OK" },
				{ "code", 204 }, // no content
			};

			try {
			
				std::vector<std::shared_ptr<ResourceCallback> > callbacks;
			
				std::unique_lock<std::mutex> resourcesLock( this->m_resourcesMutex );
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

						callbacks.push_back( *resourceIt );
					}
				}
				resourcesLock.unlock();

				for ( auto callbacksIt = callbacks.begin(); callbacksIt != callbacks.end(); callbacksIt++ ) {
					(*callbacksIt)->callback( user, input, method, output );
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
			} catch( std::exception& exception_ ) {
				output["result"] = "ERROR";
				output["code"] = 500;
				output["error"] = "Resource.Failure";
				output["message"] = "The requested resource failed to load.";
				g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
			} catch( ... ) {
				output["result"] = "ERROR";
				output["code"] = 500;
				output["error"] = "Resource.Failure";
				output["message"] = "The requested resource failed to load.";
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

	void WebServer::_installHardwareResourceHandler() {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/hardware(/([0-9]+|settings))?$",
			WebServer::Method::GET | WebServer::Method::POST | WebServer::Method::PUT | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( user_ == nullptr || user_->getRights() < User::Rights::INSTALLER ) {
					return;
				}

				auto fGetSettings = []( std::shared_ptr<Hardware> hardware_ = nullptr ) {
					if ( hardware_ == nullptr ) {
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
								{
									{ "value", "zwave" },
									{ "label", ZWave::label }
								},
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
									{ "value", "p1meter" },
									{ "label", P1Meter::label }
								},
								{
									{ "value", "telegram" },
									{ "label", Telegram::label }
								},
								{
									{ "value", "sunriset" },
									{ "label", Sunriset::label }
								},
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
							Hardware::Type type = Hardware::resolveType( hardwareData["type"].get<std::string>() );
							std::string reference = randomString( 16 );
							hardware = g_controller->declareHardware( type, reference, { } );
							hardwareId = hardware->getId();
							output_["code"] = 201; // Created
						} else {

							// The enabled property can be used to enable or disable the hardware. For now this is only
							// possible on parent/main hardware, no children.
							bool enabled = hardware->isRunning();
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
								restart = true;
							}

							std::thread( [hardware,enabled,restart]{
								auto hardwareList = g_controller->getAllHardware();
								if (
									! enabled
									|| restart
								) {
									if ( hardware->isRunning() ) {
										hardware->stop();
									}
									for ( auto hardwareIt = hardwareList.begin(); hardwareIt != hardwareList.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware
											&& (*hardwareIt)->isRunning()
										) {
											(*hardwareIt)->stop();
										}
									}
								}
								if ( enabled ) {
									if ( ! hardware->isRunning() ) {
										hardware->start();
									}
									for ( auto hardwareIt = hardwareList.begin(); hardwareIt != hardwareList.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware
											&& ! (*hardwareIt)->isRunning()
										) {
											(*hardwareIt)->start();
										}
									}
								}
							} ).detach();

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
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/devices(/([0-9]+))?$",
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
									if ( enabled && ! (*deviceIt)->isRunning() ) {
										continue;
									}
									if ( ! enabled && (*deviceIt)->isRunning() ) {
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
							deviceData.erase( "enabled" );

							g_database->putQuery(
								"UPDATE `devices` "
								"SET `enabled`=%d "
								"WHERE `id`=%d",
								enabled ? 1 : 0,
								device->getId()
							);

							device->putSettingsJson( deviceData );
							device->getSettings()->put( deviceData );
							bool restart = false;
							if ( device->getSettings()->isDirty() ) {
								device->getSettings()->commit();
								restart = true;
							}

							std::thread( [device,enabled,restart]{
								if (
									! enabled
									|| restart
								) {
									if ( device->isRunning() ) {
										device->stop();
									}
								}
								if ( enabled ) {
									if ( ! device->isRunning() ) {
										device->start();
									}
								}
							} ).detach();

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
			"^api/devices/([0-9]+)/data$",
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
			"^api/devices/([0-9]+)/links(/([0-9]+|settings))?$",
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
							&& (*devicesIt)->isRunning()
							&& (
								device->getType() != Device::Type::SWITCH
								|| ! readOnly
							)
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
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/scripts(/([0-9]+))?$",
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
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/timers(/([0-9]+))?$",
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
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/user/(login|refresh)$",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( std::shared_ptr<User> user_, const json& input_, const WebServer::Method& method_, json& output_ ) {

				json user;
			
				if (
					method_ == WebServer::Method::POST
					&& input_["$1"].get<std::string>() == "login"
				) {
				
					// The _processHttpRequest has already checked the username and password, so if no user pas passed to
					// this callback it means that either the username or password was invalid.
					if ( user_ == nullptr ) {
						throw WebServer::ResourceException( 400, "Login.Failure", "The username and/or password is invalid." );
					}
					
					// Get the rest of the user details necessary to provide a proper webtoken to the client.
					user = g_database->getQueryRow<json>(
						"SELECT `id`, `name`, `username`, `rights`, `enabled`, CAST(strftime('%%s','now','+%d minute') AS INTEGER) AS `valid`, CAST(strftime('%%s','now') AS INTEGER) AS `created` "
						"FROM `users` "
						"WHERE `id`=%d ",
						WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES,
						user_->getId()
					);
					output_["code"] = 201; // Created
				} else if (
					method_ == WebServer::Method::GET
					&& input_["$1"].get<std::string>() == "refresh"
					&& user_ != nullptr
				) {
					user = g_database->getQueryRow<json>(
						"SELECT `id`, `name`, `username`, `rights`, `enabled`, CAST(strftime('%%s','now','+%d minute') AS INTEGER) AS `valid`, CAST(strftime('%%s','now') AS INTEGER) AS `created` "
						"FROM `users` "
						"WHERE `id`=%d "
						"AND `enabled`=1",
						WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES,
						user_->getId()
					);
					output_["code"] = 200;
				} else {
					return;
				}

				json token = {
					{ "user_id", user["id"].get<unsigned int>() },
					{ "rights", user["rights"].get<unsigned int>() },
					{ "valid", user["valid"].get<unsigned int>() }
				};
				auto valid = user["valid"].get<unsigned int>();
				auto created = user["created"].get<unsigned int>();
				user.erase( "valid" );
				user.erase( "created" );
				output_["data"] = {
					{ "user", user },
					{ "valid", valid },
					{ "created", created },
					{ "token", this->_encrypt64( token.dump() ) }
				};
			} )
		) );

		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/users(/([0-9]+))?$",
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
								user["password"] = this->_hash( (*find).get<std::string>() );
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
		) );
		
		this->m_resources.push_back( std::make_shared<ResourceCallback>(
			"webserver",
			"^api/user/state/([0-9a-z_]+)$",
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
