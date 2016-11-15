#include "Worker.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;

	Worker::Worker() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be created before Worker instances." );
#endif // _DEBUG
	};

	Worker::~Worker() {
#ifdef _DEBUG
		assert( g_logger && "Global Logger instance should be destroyed after Worker instances." );
#endif // _DEBUG
	};

	bool Worker::start() {
		if ( this->m_worker.joinable() ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Already started." );
			return false;
		} else {
			g_logger->log( Logger::LogLevel::NORMAL, this, "Starting..." );
			this->m_shutdown = false;
			this->m_worker = std::thread( [&]{
				do {
					std::chrono::milliseconds wait;
					try {
						wait = this->_doWork();
					} catch( WorkerException exception_ ) {
						g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
						wait = exception_.getWait();
					} catch( std::exception exception_ ) {
						g_logger->log( Logger::LogLevel::ERROR, this, exception_.what() );
						wait = std::chrono::milliseconds( 1000 * 60 * 5 ); // 5 minutes cooldown for all unknown exceptions
					} catch( ... ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Unknown exception in worker." );
						wait = std::chrono::milliseconds( 1000 * 60 * 5 ); // 5 minutes cooldown for all unknown exceptions
					}
					std::unique_lock<std::mutex> lock( this->m_shutdownMutex );
					this->m_shutdownCondition.wait_for( lock, wait, [&]{ return this->m_shutdown; } );
				} while( ! this->m_shutdown );
				g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
			} );
		}
		return true;
	};

	bool Worker::stop() {
		if ( ! this->m_worker.joinable() ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Not running." );
			return false;
		} else {
			g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
			{
				std::lock_guard<std::mutex> lock( this->m_shutdownMutex );
				this->m_shutdown = true;
			}
			this->m_shutdownCondition.notify_all();
			if ( this->m_worker.joinable() ) {
				this->m_worker.join();
			}
		}
		return true;
	};

}; // namespace micasa
