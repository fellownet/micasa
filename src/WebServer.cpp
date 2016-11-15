#include "WebServer.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	WebServer::WebServer() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before global WebServer instance." );
		assert( g_logger && "Global Logger instance should be created before global WebServer instance." );
#endif // _DEBUG
	};

	WebServer::~WebServer() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after global WebServer instance." );
		assert( g_logger && "Global Logger instance should be destroyed after global WebServer instance." );
		assert( this->m_resources.size() == 0 && "All resources should be removed before the global WebServer instance is destroyed." );
#endif // _DEBUG
	};

	std::string WebServer::toString() const {
		return "WebServer";
	};

	void WebServer::addResourceHandler( std::string resource_, int supportedMethods_, std::shared_ptr<WebServerResource> handler_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources[resource_] = std::pair<int, std::shared_ptr<WebServerResource> >( supportedMethods_, handler_ );
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Resource no. %d at " + resource_ + " installed.", this->m_resources.size() );
	}

	void WebServer::removeResourceHandler( std::string resource_ ) {
		std::lock_guard<std::mutex> lock( this->m_resourcesMutex );
		this->m_resources.erase( resource_ );
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Resource " + resource_ + " removed, %d resources left.", this->m_resources.size() );
	}

}; // namespace micasa
