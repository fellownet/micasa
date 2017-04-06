#pragma once

#include <string>
#include <mutex>
#include <sstream>
#include <stdarg.h>

#include "Utils.h"

#define MAX_LOG_LINE_LENGTH 65536

namespace micasa {

	// ======
	// Logger
	// ======

	class Logger final {

	public:
		enum class LogLevel: int {
			ERROR   = -99,
			WARNING = -98,
			SCRIPT  = -1,
			NORMAL  = 0,
			VERBOSE = 1,
			DEBUG   = 99
		}; // enum class LogLevel
		ENUM_UTIL( LogLevel );

		// ========
		// Receiver
		// ========

		class Receiver {

		public:
			virtual void log( const LogLevel& logLevel_, const std::string& message_ ) = 0;

		}; // class Receiver

		Logger( const Logger& ) = delete; // do not copy
		Logger& operator=( const Logger& ) = delete; // do not copy-assign

		template<class T> static void logr( const LogLevel logLevel_, const T& instance_, std::string message_, ... ) {
			Logger& logger = Logger::get();
			std::stringstream message;
			message << "[" << instance_ << "] " << message_;
			va_list arguments;
			va_start( arguments, message_ );
			logger._doLog( logLevel_, message.str(), true, arguments );
			va_end( arguments );
		};
		template<class T> static void log( const LogLevel logLevel_, const T& instance_, std::string message_ ) {
			Logger& logger = Logger::get();
			std::stringstream message;
			message << "[" << instance_ << "] " << message_;
			va_list empty;
			logger._doLog( logLevel_, message.str(), false, empty );
		};

		static void addReceiver( std::shared_ptr<Receiver> receiver_, LogLevel level_ );
		static void removeReceiver( std::shared_ptr<Receiver> receiver_ );

		template<class T> static std::shared_ptr<T> addReceiver( LogLevel level_ ) {
			std::shared_ptr<T> receiver = std::make_shared<T>();
			Logger::addReceiver( receiver, level_ );
			return receiver;
		};

	private:
		struct t_logReceiver {
			std::weak_ptr<Receiver> receiver;
			LogLevel level;
			mutable unsigned short recursions;
		};
		std::vector<t_logReceiver> m_receivers;
		mutable std::recursive_mutex m_receiversMutex;

		Logger() { }; // private constructor
		~Logger() { }; // private destructor

		static Logger& get() {
			// In c++11 static initialization is supposed to be thread-safe.
			static Logger instance;
			return instance;
		}

		void _doLog( const LogLevel& logLevel_, const std::string& message_, bool hasArguments_, va_list arguments_ );

	}; // class Logger

	// =============
	// ConsoleLogger
	// =============

	class ConsoleLogger : public Logger::Receiver {

	public:
		void log( const Logger::LogLevel& logLevel_, const std::string& message_ ) override;

	}; // class ConsoleLogger

}; // namespace micasa
