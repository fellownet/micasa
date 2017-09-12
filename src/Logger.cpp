#include <cstring>
#include <iostream>

#include "Logger.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {

	// ======
	// Logger
	// ======

	void Logger::addReceiver( std::shared_ptr<Receiver> receiver_, LogLevel level_ ) {
		Logger& logger = Logger::get();
		std::lock_guard<std::mutex> lock( logger.m_receiversMutex );
		logger.m_receivers.push_back( { receiver_, level_, { "", Logger::LogLevel::NORMAL, 0 } } );
	};

	void Logger::removeReceiver( std::shared_ptr<Receiver> receiver_ ) {
		Logger& logger = Logger::get();
		std::lock_guard<std::mutex> lock( logger.m_receiversMutex );
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

		// First all log receivers eligible for this log message are gathered while the lock is held.
		std::unique_lock<std::mutex> lock( this->m_receiversMutex );
		std::vector<std::tuple<std::shared_ptr<Receiver>,LogLevel,std::string>> queue;
		for ( auto& receiverIt : this->m_receivers ) {
			std::shared_ptr<Receiver> receiver = receiverIt.receiver.lock();
#ifdef _DEBUG
			assert( receiver && "Log receivers should be removed from the logger before being destroyed." );
#endif // _DEBUG
			if (
				receiver
				&& logLevel_ <= receiverIt.level
			) {
				// Prevent logging the same message more than once per receiver. This can prevent endless-loops where
				// the logging of a message itself causes another log message.
				if (
					receiverIt.last.message == message
					&& receiverIt.last.level == logLevel_
				) {
					receiverIt.last.count++;
				} else {
					if ( receiverIt.last.count > 0 ) {
						queue.push_back( std::make_tuple( receiver, receiverIt.last.level, "Last message was repeated " + std::to_string( receiverIt.last.count ) + " times." ) );
					}
					receiverIt.last.message = message;
					receiverIt.last.level = logLevel_;
					receiverIt.last.count = 0;
					queue.push_back( std::make_tuple( receiver, logLevel_, message ) );
				}
			}
		}
		lock.unlock();

		// The actual logging is done with the lock released to make sure that any subsequent logging won't deadlock.
		for ( const auto& log : queue ) {
			std::get<0>( log )->log( std::get<1>( log ), std::get<2>( log ) );
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
			case Logger::LogLevel::NOTICE:
				std::cout << "\033[0;33m" << timebuf << message_ << "\033[0m\n";
				break;
			default:
				std::cout << timebuf << message_ << "\n";
				break;
		}
	};

} // namespace micasa
