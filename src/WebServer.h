#pragma once

#include <mutex>
#include <chrono>
#include <map>
#include <vector>

#include "Utils.h"
#include "Network.h"
#include "Scheduler.h"

#include "json.hpp"

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

extern "C" {
	#include "mongoose.h"
} // extern "C"

#define WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES 10080
#define WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX        "_web_"

namespace micasa {

	class User;

	class WebServer final {

	public:
		enum class Method: unsigned short {
			// v Retrieve all resources in a collection
			// v Retrieve a single resource
			GET     = 1,
			// v Retrieve all resources in a collection (header only)
			// v Retrieve a single resource (header only)
			HEAD    = 2,
			// v Create a new resource in a collection
			POST    = 4,
			// v Update a resource
			PUT     = 8,
			// v Update a resource
			PATCH   = 16,
			// v Delete a resource
			DELETE  = 32,
			// v Return available methods for resource or collection
			OPTIONS = 64
		}; // enum class Method
		static const std::map<Method, std::string> MethodText;
		ENUM_UTIL_W_TEXT( Method, MethodText );
		
		typedef std::function<void( std::shared_ptr<User> user_, const nlohmann::json& input_, const Method& method_, nlohmann::json& output_ )> t_callback;
		
		class ResourceCallback {
		public:
			ResourceCallback( const std::string& reference_, const std::string& uri_, const Method& methods_, const t_callback& callback_ ) : reference( reference_ ), uri( uri_ ), methods( methods_ ), callback( callback_ ) { };

			const std::string reference;
			const std::string uri;
			const Method methods;
			const t_callback callback;
		}; // class ResourceCallback
		
		class ResourceException: public std::runtime_error {
		public:
			ResourceException( unsigned int code_, std::string error_, std::string message_ ) : runtime_error( message_ ), code( code_ ), error( error_ ), message( message_ ) { };
			
			const unsigned int code;
			const std::string error;
			const std::string message;
		}; // class ResourceException

		WebServer();
		~WebServer();
		friend std::ostream& operator<<( std::ostream& out_, const WebServer* ) { out_ << "WebServer"; return out_; }
		
		void start();
		void stop();
		
	//protected:
		//std::chrono::milliseconds _work( const unsigned long int& iteration_ ) { return std::chrono::milliseconds( 1000 * 60 * 15 ); };
		
	private:
		Scheduler m_scheduler;
		std::vector<std::shared_ptr<ResourceCallback> > m_resources;
		mutable std::mutex m_resourcesMutex;
		std::shared_ptr<Network::Connection> m_bind;

#ifdef _WITH_OPENSSL
		std::shared_ptr<Network::Connection> m_bindSecure;
		X509* m_certificate;
		EVP_PKEY* m_key; // private key

		std::string _encrypt64( const std::string& data_ ) const;
		std::string _decrypt64( const std::string& data_ ) const;
#endif
		std::string _hash( const std::string& data_ ) const;

		void _processRequest( Network::Connection& connection_, const std::string& data_ );
		
		void _installHardwareResourceHandler();
		void _installDeviceResourceHandler();
		void _installScriptResourceHandler();
		void _installTimerResourceHandler();
		void _installUserResourceHandler();

	}; // class WebServer

}; // namespace micasa
