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
		"Usage: micasa [-p|--port <port>] [-sslp|--sslport <port>] [-l|--loglevel <loglevel>]\n"
		"\t-p|--port <port>\n\t\tSets the port for web connections (defaults to 80).\n"
		"\t-sslp|--sslport <port>\n\t\tSets the port for secure web connections (defaults to no ssl).\n"
		"\t-l|--loglevel <loglevel>\n\t\tSets the level of logging:\n"
		"\t\t\t0 = default\n"
		"\t\t\t1 = verbose\n"
		"\t\t\t99 = debug\n"
	;

	static volatile bool g_shutdown = false;

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

	int port = 80;
	if ( arguments.exists( "-p" ) ) {
		port = atoi( arguments.get( "-p" ).c_str() );
	} else if ( arguments.exists( "--port" ) ) {
		port = atoi( arguments.get( "--port" ).c_str() );
	}

	int sslport = 0;
	if ( arguments.exists( "-sslp" ) ) {
		sslport = atoi( arguments.get( "-sslp" ).c_str() );
	} else if ( arguments.exists( "--sslport" ) ) {
		sslport = atoi( arguments.get( "--sslport" ).c_str() );
	}

	Logger::LogLevel logLevel = Logger::LogLevel::NORMAL;
	if ( arguments.exists( "-l" ) ) {
		logLevel = Logger::resolveLogLevel( std::stoi( arguments.get( "-l" ) ) );
	} else if ( arguments.exists( "--loglevel" ) ) {
		logLevel = Logger::resolveLogLevel( std::stoi( arguments.get( "--loglevel" ) ) );
	}
	auto logger = Logger::addReceiver<ConsoleLogger>( logLevel );

	// See if the datadir is read- and writable.
	struct stat info;
	if (
		stat( _DATADIR, &info ) != 0
		|| ( info.st_mode & S_IFDIR ) == 0
	) {
		std::cout << "Data directory is not read-writable (" << _DATADIR << ").\n";
		return EXIT_FAILURE;
	}

	struct sigaction action;
	memset( &action, 0, sizeof( struct sigaction ) );
	action.sa_handler = signal_handler;
	sigaction( SIGINT, &action, NULL );
	sigaction( SIGTERM, &action, NULL );

	g_database = std::unique_ptr<Database>( new Database );

	// The database might take some time to initialize (due to the VACUUM call). An additional shutdown check is done.
	if ( ! g_shutdown ) {
		g_settings = std::unique_ptr<Settings<>>( new Settings<> );
		g_controller = std::unique_ptr<Controller>( new Controller );
		g_webServer = std::unique_ptr<WebServer>( new WebServer( port, sslport ) );

		g_controller->start();
		g_webServer->start();

		while ( ! g_shutdown ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
		}

		g_webServer->stop();
		std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
		g_controller->stop();
		std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );

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
