#include <cstring>
#include <iostream>

#include "Logger.h"

namespace micasa {

	void Logger::_doLog( const LogLevel logLevel_, std::string message_, bool hasArguments_, va_list arguments_ ) const {
		std::lock_guard<std::mutex> lock( this->m_logMutex );
		if ( logLevel_ <= this->m_logLevel ) {
			char buffer[MAX_LOG_LINE_LENGTH];
			if ( ! hasArguments_ ) {
				strcpy( buffer, message_.c_str() );
			} else {
				vsnprintf( buffer, sizeof( buffer ), message_.c_str(), arguments_ );
			}

			time_t now = time( 0 );
			struct tm tstruct;
			char timebuf[80];
			tstruct = *localtime(&now);
			strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S ", &tstruct);

			switch( logLevel_ ) {
				case LogLevel::WARNING:
					std::cout << "\033[0;36m" << timebuf << buffer << "\033[0m\n";
					break;
				case LogLevel::ERROR:
					std::cout << "\033[0;31m" << timebuf << buffer << "\033[0m\n";
					break;
				case LogLevel::VERBOSE:
				case LogLevel::DEBUG:
					std::cout << "\033[0;37m" << timebuf << buffer << "\033[0m\n";
					break;
				default:
					std::cout << timebuf << buffer << "\n";
					break;
			}
		}
	};

	void Logger::logr( const LogLevel logLevel_, std::string message_, ... ) const {
		va_list arguments;
		va_start( arguments, message_ );
		this->_doLog( logLevel_, message_.c_str(), true, arguments );
		va_end( arguments );
	};

} // namespace micasa
