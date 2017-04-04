#include <cstring>
#include <iostream>

#include "Logger.h"

namespace micasa {

	// ======
	// Logger
	// ======

	void Logger::addReceiver( std::shared_ptr<Receiver> receiver_, LogLevel level_ ) {
		Logger& logger = Logger::get();
		std::lock_guard<std::recursive_mutex> lock( logger.m_logMutex );
		logger.m_receivers.push_back( { receiver_, level_, 0 } );
	};
	
	void Logger::removeReceiver( std::shared_ptr<Receiver> receiver_ ) {
		Logger& logger = Logger::get();
		std::lock_guard<std::recursive_mutex> lock( logger.m_logMutex );
		for ( auto receiversIt = logger.m_receivers.begin(); receiversIt != logger.m_receivers.end(); ) {
			if ( (*receiversIt).receiver.lock() == receiver_ ) {
				receiversIt = logger.m_receivers.erase( receiversIt );
			} else {
				receiversIt++;	
			}
		}
	};

	void Logger::_doLog( const LogLevel& logLevel_, const std::string& message_, bool hasArguments_, va_list arguments_ ) {
		std::lock_guard<std::recursive_mutex> lock( this->m_logMutex );
		char buffer[MAX_LOG_LINE_LENGTH];
		if ( ! hasArguments_ ) {
			strncpy( buffer, message_.c_str(), sizeof( buffer ) );
		} else {
			vsnprintf( buffer, sizeof( buffer ), message_.c_str(), arguments_ );
		}
		for ( auto receiversIt = this->m_receivers.begin(); receiversIt != this->m_receivers.end(); ) {
			std::shared_ptr<Receiver> receiver = (*receiversIt).receiver.lock();
			if ( receiver ) {
				if (
					logLevel_ <= (*receiversIt).level
					&& (*receiversIt).recursions < 5
				) {
					(*receiversIt).recursions++;
					receiver->log( logLevel_, buffer );
					(*receiversIt).recursions--;
				}
				receiversIt++;
			} else {
				receiversIt = this->m_receivers.erase( receiversIt );
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
