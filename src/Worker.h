#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace micasa {

	class Worker {

	public:
		Worker() { };
		~Worker();
		
		bool isRunning();
		void notify();

	protected:
		virtual void _begin();
		virtual void _retire();
		virtual std::chrono::milliseconds _work( unsigned long int iteration_ ) =0;

	private:
		std::thread m_worker;
		volatile bool m_shutdown = true;
		std::condition_variable m_shutdownCondition;
		std::mutex m_shutdownMutex;
		unsigned long int m_iteration = 0;

	}; // class Worker

}; // namespace micasa
