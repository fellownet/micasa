#include <cstring>
#include <iostream>

#include "Logger.h"

namespace micasa {

	// ======
	// Logger
	// ======

	void Logger::addReceiver( std::shared_ptr<Receiver> receiver_, LogLevel level_ ) {
		Logger& logger = Logger::get();
		std::lock_guard<std::recursive_mutex> receiversLock( logger.m_receiversMutex );
		logger.m_receivers.push_back( { receiver_, level_ } );
	};
	
	void Logger::removeReceiver( std::shared_ptr<Receiver> receiver_ ) {
		Logger& logger = Logger::get();
		std::lock_guard<std::recursive_mutex> receiversLock( logger.m_receiversMutex );
		for ( auto receiversIt = logger.m_receivers.begin(); receiversIt != logger.m_receivers.end(); ) {
			if ( (*receiversIt).receiver.lock() == receiver_ ) {
				receiversIt = logger.m_receivers.erase( receiversIt );
			} else {
				receiversIt++;	
			}
		}
	};

	void Logger::_doLog( const LogLevel& logLevel_, const std::string& message_, bool hasArguments_, va_list arguments_ ) {
		char buffer[MAX_LOG_LINE_LENGTH];
		if ( ! hasArguments_ ) {
			strncpy( buffer, message_.c_str(), sizeof( buffer ) );
		} else {
			vsnprintf( buffer, sizeof( buffer ), message_.c_str(), arguments_ );
		}
		std::string message( buffer );

		std::unique_lock<std::recursive_mutex> receiversLock( this->m_receiversMutex );
		std::vector<std::reference_wrapper<t_logReceiver> > receivers( this->m_receivers.begin(), this->m_receivers.end() );
		receiversLock.unlock();
		
		for ( auto receiversIt = receivers.begin(); receiversIt != receivers.end(); receiversIt++ ) {
			std::shared_ptr<Receiver> receiver = (*receiversIt).get().receiver.lock();
			if (
				receiver
				&& logLevel_ <= (*receiversIt).get().level
			) {
				receiver->log( logLevel_, message );
			}
		}
	};

	// =============
	// ConsoleLogger
	// =============

	void ConsoleLogger::log( const Logger::LogLevel& logLevel_, const std::string& message_ ) {
		time_t now = time( 0 );
		struct tm tstruct;
		char timebuf[80];
		tstruct = *localtime(&now);
		strftime( timebuf, sizeof( timebuf ), "%Y-%m-%d %H:%M:%S ", &tstruct );

		switch( logLevel_ ) {
			case Logger::LogLevel::WARNING:
				std::cerr << "\033[0;36m" << timebuf << message_ << "\033[0m\n";
				break;
			case Logger::LogLevel::ERROR:
				std::cerr << "\033[0;31m" << timebuf << message_ << "\033[0m\n";
				break;
			case Logger::LogLevel::VERBOSE:
			case Logger::LogLevel::DEBUG:
				std::cout << "\033[0;37m" << timebuf << message_ << "\033[0m\n";
				break;
			case Logger::LogLevel::SCRIPT:
				std::cout << "\033[0;33m" << timebuf << message_ << "\033[0m\n";
				break;
			default:
				std::cout << timebuf << message_ << "\n";
				break;
		}
	};

} // namespace micasa
