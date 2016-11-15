#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "Logger.h"

namespace micasa {

	class WorkerException : public std::runtime_error {

	public:
		WorkerException( const std::string &message_, std::chrono::milliseconds wait_ ) : std::runtime_error( message_ ), m_wait( wait_ ) { };
		
		std::chrono::milliseconds getWait() const { return this->m_wait; }

	private:
		std::chrono::milliseconds m_wait;

	}; // class Exception

	class Worker : public LoggerInstance {

	public:
		Worker();
		~Worker();
		
		virtual bool start();
		virtual bool stop();

	protected:
		virtual std::chrono::milliseconds _doWork() =0;

	private:
		std::thread m_worker;
		volatile bool m_shutdown;
		std::condition_variable m_shutdownCondition;
		std::mutex m_shutdownMutex;

	}; // class Worker

}; // namespace micasa
