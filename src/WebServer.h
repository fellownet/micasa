#pragma once

#include <mutex>
#include <chrono>
#include <map>
#include <vector>
#include <ostream>

#include "Utils.h"
#include "Network.h"
#include "Scheduler.h"

#include "json.hpp"

#define WEBSERVER_TOKEN_DEFAULT_VALID_DURATION_MINUTES 10080
#define WEBSERVER_USER_WEBCLIENT_SETTING_PREFIX "_web_"
#define WEBSERVER_SETTING_HASH_PEPPER "_hash_pepper"

namespace micasa {

	class User;

	class WebServer final {

	public:
		enum class Method: unsigned short {
			// v Retrieve all resources in a collection
			// v Retrieve a single resource
			GET     = (1 << 0),
			// v Retrieve all resources in a collection (header only)
			// v Retrieve a single resource (header only)
			HEAD    = (1 << 1),
			// v Create a new resource in a collection
			POST    = (1 << 2),
			// v Update a resource
			PUT     = (1 << 3),
			// v Update a resource
			PATCH   = (1 << 4),
			// v Delete a resource
			DELETE  = (1 << 5),
			// v Return available methods for resource or collection
			OPTIONS = (1 << 6)
		}; // enum class Method
		static const std::map<Method, std::string> MethodText;
		ENUM_UTIL_W_TEXT( Method, MethodText );
		
		typedef std::function<void( std::shared_ptr<User> user_, const nlohmann::json& input_, const Method& method_, nlohmann::json& output_ )> t_callback;
		
		class ResourceCallback {
		public:
			ResourceCallback( const std::string& uri_, const Method& methods_, const t_callback& callback_ ) : uri( uri_ ), methods( methods_ ), callback( callback_ ) { };

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

		WebServer( unsigned int port_, unsigned int sslport_ );
		~WebServer();

		WebServer( const WebServer& ) = delete; // Do not copy!
		WebServer& operator=( const WebServer& ) = delete; // Do not copy-assign!
		WebServer( const WebServer&& ) = delete; // do not move
		WebServer& operator=( WebServer&& ) = delete; // do not move-assign

		friend std::ostream& operator<<( std::ostream& out_, const WebServer* ) { out_ << "WebServer"; return out_; }
		
		void start();
		void stop();

	private:
		unsigned int m_port;
		unsigned int m_sslport;
		Scheduler m_scheduler;
		std::vector<std::shared_ptr<ResourceCallback>> m_resources;
		std::map<std::string, std::pair<std::chrono::system_clock::time_point, std::shared_ptr<User>>> m_logins;
		mutable std::mutex m_loginsMutex;
		std::shared_ptr<Network::Connection> m_bind;
		std::shared_ptr<Network::Connection> m_sslbind;

		std::string _hash( const std::string& data_ ) const;
		void _processRequest( std::shared_ptr<Network::Connection> connection_ );
		void _installHardwareResourceHandler();
		void _installDeviceResourceHandler();
		void _installLinkResourceHandler();
		void _installScriptResourceHandler();
		void _installTimerResourceHandler();
		void _installUserResourceHandler();

	}; // class WebServer

}; // namespace micasa
