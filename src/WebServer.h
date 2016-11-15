#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <mutex>
#include <chrono>
#include <map>

#include "Worker.h"
#include "Logger.h"
#include "Database.h"

namespace micasa {

	class WebServerResource {

	public:

		typedef enum {
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
		} Method;

		virtual bool handleRequest( std::string resource_, Method method_, std::map<std::string, std::string> &data_ ) =0;

	}; // class LoggerInstance

	class WebServer final : public LoggerInstance {

	public:
		WebServer();
		~WebServer();
		
		std::string toString() const;
		
		void addResourceHandler( std::string resource_, int supportedMethods_, std::shared_ptr<WebServerResource> handler_ );
		void removeResourceHandler( std::string resource_ );

	private:
		std::map<std::string, std::pair<int, std::shared_ptr<WebServerResource> > > m_resources;
		std::mutex m_resourcesMutex;

	}; // class WebServer

}; // namespace micasa
