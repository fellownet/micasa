#pragma once

#include <string>
#include <mutex>

#define MAX_LOG_LINE_LENGTH 2048

namespace micasa {

	class LoggerInstance {

	public:
		virtual std::string toString() const =0;

	}; // class LoggerInstance

	class Logger final {

	public:
		typedef enum {
			ERROR = -99,
			WARNING = -98,
			NORMAL = 0,
			VERBOSE,
			DEBUG,
		} LogLevel;

		Logger( const LogLevel logLevel_ ) : m_logLevel( logLevel_ ) { };
		~Logger() { };
		
		void log( const LogLevel logLevel_, std::string message_, ... ) const;
		void log( const LogLevel logLevel_, const LoggerInstance* instance_, std::string message_, ... ) const;
		void logRaw( const LogLevel logLevel_, const LoggerInstance* instance_, std::string message_ ) const;
		
	private:
		LogLevel m_logLevel;
		mutable std::mutex m_logMutex;

		void _doLog( const LogLevel logLevel_, std::string message_, va_list arguments_ ) const;


	}; // class Logger

}; // namespace micasa
