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
		
		bool isRunning() const;
		void wakeUp();

		virtual void start();
		virtual void stop();

	protected:
		volatile bool m_shutdown = true;
		
		virtual std::chrono::milliseconds _work( const unsigned long int& iteration_ ) =0;
		void _synchronize( std::function<void()> func_ ) const;

	private:
		std::thread m_worker;
		std::condition_variable m_continueCondition;
		mutable std::mutex m_conditionMutex;
		mutable std::mutex m_workMutex;
		unsigned long int m_iteration = 0;
		volatile bool m_hasWork = false;
		
	}; // class Worker

}; // namespace micasa
