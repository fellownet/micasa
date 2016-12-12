#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <mutex>
#include <chrono>
#include <map>
#include <vector>

#include "Worker.h"
#include "Network.h"

#include "json.hpp"

namespace micasa {

	class WebServer final : public Worker {

	public:
		enum Method {
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
		
		typedef std::function<void( const std::string& uri_, const std::map<std::string, std::string>& input_, const Method& method_, int& code_, nlohmann::json& output_ )> t_callback;
		
		struct ResourceCallback {
			const std::string reference;
			const std::string uri;
			const unsigned int methods;
			const t_callback callback;
		};
		
		struct ResourceCache {
			const std::string content;
			const std::string etag;
			const std::string modified;
		};
		
		WebServer();
		~WebServer();
		friend std::ostream& operator<<( std::ostream& out_, const WebServer* ) { out_ << "WebServer"; return out_; }
		
		void addResourceCallback( std::shared_ptr<ResourceCallback> callback_ );
		void removeResourceCallback( const std::string reference_ );
		void touchResourceCallback( const std::string reference_ );
		// TODO remove vv
		void touchResourceAt( const std::string uri_ );

		void start();
		void stop();
		
	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		
	private:
		std::map<std::string, std::vector<std::shared_ptr<ResourceCallback> > > m_resources;
		std::map<std::string, ResourceCache> m_resourceCache;
		mutable std::mutex m_resourcesMutex;

		void _removeResourceCache( const std::string uri_ );
		void _processHttpRequest( mg_connection* connection_, http_message* message_ );

	}; // class WebServer

}; // namespace micasa
