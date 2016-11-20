#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <mutex>
#include <chrono>
#include <map>

#include "Worker.h"
#include "Logger.h"

#include "json.hpp"

extern "C" {
	
	#include "mongoose.h"
	
	static void mongoose_handler( mg_connection* connection_, int event_, void* instance_ );
	
} // extern "C"

namespace micasa {

	class WebServer final : public Worker, public LoggerInstance {

		friend void ::mongoose_handler( struct mg_connection *connection_, int event_, void* instance_ );

	public:
		enum ResourceMethod {
			// v Retrieve all resources in a collection
			// v Retrieve a single resource
			GET = 1,
			// v Retrieve all resources in a collection (header only)
			// v Retrieve a single resource (header only)
			HEAD = 2,
			// v Create a new resource in a collection
			POST = 4,
			// v Update a resource
			PUT = 8,
			// v Update a resource
			PATCH = 16,
			// v Delete a resource
			DELETE = 32,
			// v Return available methods for resource or collection
			OPTIONS = 64
		}; // enum Method

		class ResourceHandler; // forward declaration
		
		struct Resource {
			std::string uri;
			int methods;
			std::string title;
			std::shared_ptr<ResourceHandler> handler;
		};

		class ResourceHandler {
			
		public:
			virtual void handleResource( const Resource& resource_, int& code_, nlohmann::json& output_ ) =0;
		
		}; // class ResourceHandler

		WebServer();
		~WebServer();
		
		std::string toString() const;

		void addResource( Resource resource_ );
		void removeResourceAt( std::string uri_ );
		void touchResourceAt( std::string uri_ );

		void start();
		void stop();

	protected:
		std::chrono::milliseconds _work( unsigned long int iteration_ );
		
	private:
		mg_mgr m_manager;
		mg_connection* m_connection;
		
		std::map<std::string, std::pair<Resource, std::string> > m_resources;
		std::mutex m_resourcesMutex;

		void _processHttpRequest( mg_connection* connection_, http_message* message_ );

	}; // class WebServer

}; // namespace micasa
