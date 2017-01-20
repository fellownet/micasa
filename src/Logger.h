#pragma once

#include <string>
#include <mutex>
#include <sstream>
#include <stdarg.h>

#include "Utils.h"

#define MAX_LOG_LINE_LENGTH 2048

namespace micasa {

	class Logger final {

	public:
		enum class LogLevel: int {
			ERROR = -99,
			WARNING = -98,
			NORMAL = 0,
			VERBOSE,
			DEBUG,
		}; // enum class LogLevel
		ENUM_UTIL( LogLevel );

		Logger( const LogLevel logLevel_ ) : m_logLevel( logLevel_ ) { };
		~Logger() { };

		void logr( const LogLevel logLevel_, std::string message_, ... ) const;
		
		// Unfortunately template based methods need to be implemented in the header file for it to accept *all*
		// types of classes.
		template<class T> void logr( const LogLevel logLevel_, const T& instance_, std::string message_, ... ) const {
			std::stringstream message;
			message << "[" << instance_ << "] " << message_;
			va_list arguments;
			va_start( arguments, message_ );
			this->_doLog( logLevel_, message.str(), true, arguments );
			va_end( arguments );
		};
		template<class T> void log( const LogLevel logLevel_, const T& instance_, std::string message_ ) const {
			std::stringstream message;
			message << "[" << instance_ << "] " << message_;
			va_list empty;
			this->_doLog( logLevel_, message.str(), false, empty );
		};

	private:
		LogLevel m_logLevel;
		mutable std::mutex m_logMutex;

		void _doLog( const LogLevel logLevel_, std::string message_, bool hasArguments_, va_list arguments_ ) const;

	}; // class Logger

}; // namespace micasa
