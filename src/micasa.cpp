#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>

#include "Arguments.h"
#include "Logger.h"
#include "Database.h"
#include "Network.h"
#include "WebServer.h"
#include "Controller.h"
#include "Settings.h"

namespace micasa {
	
	std::shared_ptr<Logger> g_logger;
	std::shared_ptr<Database> g_database;
	std::shared_ptr<Settings<> > g_settings;
	std::shared_ptr<Network> g_network;
	std::shared_ptr<WebServer> g_webServer;
	std::shared_ptr<Controller> g_controller;

	const char g_usage[] = "Usage: micasa -p|--port <port> [-l|--loglevel <loglevel>] [-d|--daemonize] [-db|--database <filename>]\n";

	bool g_shutdown = false;

	void signal_handler( int signal_ ) {
		switch( signal_ ) {
			case SIGINT:
			case SIGTERM:
				g_logger->log( Logger::LogLevel::WARNING, g_controller, "Shutting down." );
				g_shutdown = true;
				break;
		}
	};

}; // namespace micasa

using namespace micasa;

int main( int argc_, char* argv_[] ) {

	Arguments arguments( argc_, argv_ );

	if (
		arguments.exists( "-h" )
		|| arguments.exists( "--help" )
	) {
		std::cout << g_usage;
		return 0;
	}
/*
	int port = 80;
	if ( arguments.exists( "-p" ) ) {
		port = atoi( arguments.get( "-p" ).c_str() );
	} else if ( arguments.exists( "--port" ) ) {
		port = atoi( arguments.get( "--port" ).c_str() );
	}
*/
	Logger::LogLevel logLevel = Logger::LogLevel::NORMAL;
	if ( arguments.exists( "-l" ) ) {
		logLevel = Logger::resolveLogLevel( std::stoi( arguments.get( "-l" ) ) );
	} else if ( arguments.exists( "--loglevel" ) ) {
		logLevel = Logger::resolveLogLevel( std::stoi( arguments.get( "--loglevel" ) ) );
	}

	bool daemonize = false;
	if (
		arguments.exists( "-d" )
		|| arguments.exists( "--daemonize" )
	) {
		daemonize = true;
	}

	std::string database;
	if ( arguments.exists( "-db" ) ) {
		database = arguments.get( "-db" );
	} else if ( arguments.exists( "--database" ) ) {
		database = arguments.get( "--database" );
	} else {
		database = "micasa.db";
	}

	if ( ! daemonize ) {
		signal( SIGINT, signal_handler );
		signal( SIGTERM, signal_handler );
	}

	g_logger = std::make_shared<Logger>( logLevel );
	g_database = std::make_shared<Database>( database );
	g_settings = std::make_shared<Settings<> >();
	g_network = std::make_shared<Network>();
	g_controller = std::make_shared<Controller>();
	g_webServer = std::make_shared<WebServer>();

	g_network->start();
	g_controller->start();
	g_webServer->start();

	while ( ! g_shutdown ) 	{
		std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	}

	g_webServer->stop();
	g_controller->stop();
	g_network->stop();
	
	g_webServer = NULL;
	g_controller = NULL;
	g_network = NULL;
	if ( g_settings->isDirty() ) {
		g_settings->commit();
	}
	g_settings = NULL;
	g_database = NULL;
	g_logger = NULL;
	
	return EXIT_SUCCESS;
};
