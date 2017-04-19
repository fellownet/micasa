#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>

#include "Arguments.h"
#include "Logger.h"
#include "Database.h"
#include "WebServer.h"
#include "Controller.h"
#include "Settings.h"
#include "Scheduler.h"

namespace micasa {
	
	std::unique_ptr<Database> g_database;
	std::unique_ptr<Settings<>> g_settings;
	std::unique_ptr<WebServer> g_webServer;
	std::unique_ptr<Controller> g_controller;

	const char g_usage[] =
		"Usage: micasa [-p|--port <port>] [-l|--loglevel <loglevel>] [-db|--database <filename>]\n"
		"\t-p|--port <port>\n\t\tSets the port for secure web connections (defaults to 443).\n"
		"\t-l|--loglevel <loglevel>\n\t\tSets the level of logging:\n"
		"\t\t\t0 = default\n"
		"\t\t\t1 = verbose\n"
		"\t\t\t99 = debug\n"
		"\t-db|--database <filename>\n\t\tUse supplied database (defaults to micasa.db in current folder).\n"
	;

	bool g_shutdown = false;

	void signal_handler( int signal_ ) {
		switch( signal_ ) {
			case SIGINT:
			case SIGTERM:
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

	int port = 443;
	if ( arguments.exists( "-p" ) ) {
		port = atoi( arguments.get( "-p" ).c_str() );
	} else if ( arguments.exists( "--port" ) ) {
		port = atoi( arguments.get( "--port" ).c_str() );
	}

	Logger::LogLevel logLevel = Logger::LogLevel::NORMAL;
	if ( arguments.exists( "-l" ) ) {
		logLevel = Logger::resolveLogLevel( std::stoi( arguments.get( "-l" ) ) );
	} else if ( arguments.exists( "--loglevel" ) ) {
		logLevel = Logger::resolveLogLevel( std::stoi( arguments.get( "--loglevel" ) ) );
	}
	auto logger = Logger::addReceiver<ConsoleLogger>( logLevel );

	std::string database;
	if ( arguments.exists( "-db" ) ) {
		database = arguments.get( "-db" );
	} else if ( arguments.exists( "--database" ) ) {
		database = arguments.get( "--database" );
	} else {
		database = "micasa.db";
	}

	signal( SIGINT, signal_handler );
	signal( SIGTERM, signal_handler );

	g_database = std::unique_ptr<Database>( new Database( database ) );

	// The database might take some time to initialize (due to the VACUUM call). An additional shutdown check is done.
	if ( ! g_shutdown ) {
		g_settings = std::unique_ptr<Settings<>>( new Settings<> );
		g_controller = std::unique_ptr<Controller>( new Controller );
		g_webServer = std::unique_ptr<WebServer>( new WebServer( port ) );

		g_controller->start();
		g_webServer->start();

		while ( ! g_shutdown ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
		}

		g_webServer->stop();
		g_controller->stop();

		g_webServer = nullptr;
		g_controller = nullptr;
		if ( g_settings->isDirty() ) {
			g_settings->commit();
		}
		g_settings = nullptr;
	}

	g_database = nullptr;

	return EXIT_SUCCESS;
};
