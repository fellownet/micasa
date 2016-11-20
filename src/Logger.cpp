#include <cstring>
#include <stdarg.h>
#include <iostream>
#include <sstream>

#include "Logger.h"

namespace micasa {

	void Logger::_doLog( const LogLevel logLevel_, std::string message_, va_list arguments_ ) const {
		std::lock_guard<std::mutex> lock( this->m_logMutex );
		if ( logLevel_ <= this->m_logLevel ) {
			char buffer[MAX_LOG_LINE_LENGTH];
			if ( NULL == arguments_ ) {
				strcpy( buffer, message_.c_str() );
			} else {
				vsnprintf( buffer, sizeof( buffer ), message_.c_str(), arguments_ );
			}

			switch( logLevel_ ) {
				case LogLevel::WARNING:
					std::cout << "\033[0;36m" << buffer << "\033[0m\n";
					break;
				case LogLevel::ERROR:
					std::cout << "\033[0;31m" << buffer << "\033[0m\n";
					break;
				case LogLevel::VERBOSE:
				case LogLevel::DEBUG:
					std::cout << "\033[0;37m" << buffer << "\033[0m\n";
					break;
				default:
					std::cout << buffer << "\n";
					break;
			}
		}
	};

	void Logger::log( const LogLevel logLevel_, std::string message_, ... ) const {
		va_list arguments;
		va_start( arguments, message_ );
		this->_doLog( logLevel_, message_.c_str(), arguments );
		va_end( arguments );
	};

	void Logger::log( const LogLevel logLevel_, const LoggerInstance* instance_, std::string message_, ... ) const {
		std::stringstream message;
		message << "[" << instance_->toString() << "] " << message_;

		va_list arguments;
		va_start( arguments, message_ );
		this->_doLog( logLevel_, message.str(), arguments );
		va_end( arguments );
	};

	void Logger::logRaw( const LogLevel logLevel_, const LoggerInstance* instance_, std::string message_ ) const {
		std::stringstream message;
		message << "[" << instance_->toString() << "] " << message_;
		this->_doLog( logLevel_, message.str(), NULL );
	};
	
} // namespace micasa
