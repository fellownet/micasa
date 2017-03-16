#pragma once

#include <thread>
#include <future>
#include <chrono>
#include <queue>
#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include <climits>

#define SCHEDULER_INFINITE ULONG_MAX
#define SCHEDULER_INTERVAL_HOUR 1000 * 60 * 60
#define SCHEDULER_INTERVAL_5MIN 1000 * 60 * 5

namespace micasa {

	class Scheduler final {
		class ThreadPool; // forward declaration for friend statement
	
	public:
		class Task {
			friend class ThreadPool;
		
		public:
			typedef std::function<void(std::shared_ptr<Task>)> t_taskFunc;

			std::chrono::steady_clock::time_point time;
			unsigned long delay;
			unsigned long repeat;
			unsigned long iteration;
			void* data;

			Task( t_taskFunc&& func_, std::chrono::steady_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ );
			Task( const Task& ) = delete;
			Task( Task&& ) = delete;
			Task& operator=( const Task& ) = delete;
			Task& operator=( Task&& ) = delete;

			void proceed( unsigned long wait_ = 0 );
		
		private:
			Scheduler* m_scheduler;
			t_taskFunc m_func;
			std::shared_ptr<std::promise<void> > m_promise;

		}; // class Task

		Scheduler();
		~Scheduler();

		Scheduler( const Scheduler& ) = delete;
		Scheduler& operator=( const Scheduler& ) = delete;

		std::shared_ptr<Task> schedule( unsigned long delay_, unsigned long repeat_, void* data_, Task::t_taskFunc&& func_ );
		std::shared_ptr<Task> schedule( unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task> task_ );
		std::shared_ptr<Task> schedule( std::chrono::steady_clock::time_point time_, void* data_, Task::t_taskFunc&& func_ );
		std::shared_ptr<Task> schedule( std::chrono::steady_clock::time_point time_, std::shared_ptr<Task> task_ );
		void notify();

	private:
		class TaskComparator {
		
		public:
			bool operator()( const std::shared_ptr<Task> a_, const std::shared_ptr<Task> b_ ) {
				return ( a_->time > b_->time );
			}

		}; // class TaskComparator

		class ThreadPool final {

		public:
			ThreadPool( const ThreadPool& ) = delete; // Do not copy!
			ThreadPool& operator=( const ThreadPool& ) = delete; // Do not copy-assign!

			void addTask( Scheduler* scheduler_, std::shared_ptr<Task> task_ );
			void purgeTasks( Scheduler* scheduler_ );
			void notify();

			static ThreadPool& get() {
				// In c++11 static initialization is supposed to be thread-safe.
				static ThreadPool instance;
				return instance;
			}

		private:
			bool m_shutdown;
			bool m_continue;
			
			// Every mutation to m_tasks, the activeTask map and the Task objects therein, including adding a promise,
			// should be done while holding a lock on the taskMutex.
			std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task> >, TaskComparator> m_tasks;
			std::map<Scheduler*, std::vector<std::shared_ptr<Task> > > m_activeTasks;
			mutable std::mutex m_tasksMutex;
			
			std::vector<std::thread> m_threads;
			std::condition_variable m_continueCondition;
			mutable std::mutex m_conditionMutex;
			
			ThreadPool(); // private constructor
			~ThreadPool(); // private destructor
			
			void _threadLoop( unsigned int index_ );
			void _notify( bool bAll_, std::function<void()>&& func_ );

		}; // class ThreadPool

		unsigned int m_id;

	}; // class Scheduler

}; // namespace micasa
