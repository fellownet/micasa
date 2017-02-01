#pragma once

#include <mutex>
#include <chrono>
#include <map>
#include <vector>

#include "Utils.h"
#include "Worker.h"

#include "json.hpp"

extern "C" {
	#include "mongoose.h"
} // extern "C"

#define WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES 10080
#define WEBSERVER_USER_WEBCLIENT_SETTING               "_webclient"

namespace micasa {

	class User;

	class WebServer final : public Worker {

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
		ENUM_UTIL( Method );
		
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
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) { return std::chrono::milliseconds( 1000 * 60 * 15 ); };
		
	private:
		std::vector<std::shared_ptr<ResourceCallback> > m_resources;
		mutable std::mutex m_resourcesMutex;
		std::string m_publicKey;
		std::string m_privateKey;

		void _processHttpRequest( mg_connection* connection_, http_message* message_ );
		
		void _installHardwareResourceHandler();
		void _installDeviceResourceHandler();
		void _installScriptResourceHandler();
		void _installTimerResourceHandler();
		void _installUserResourceHandler();

	}; // class WebServer

}; // namespace micasa
