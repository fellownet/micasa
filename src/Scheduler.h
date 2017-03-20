#pragma once

#include <thread>
#include <future>
#include <chrono>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <climits>

#define SCHEDULER_INFINITE ULONG_MAX
#define SCHEDULER_INTERVAL_HOUR 1000 * 60 * 60
#define SCHEDULER_INTERVAL_5MIN 1000 * 60 * 5

namespace micasa {

	// =========
	// Scheduler
	// =========

	class Scheduler final {
	
	public:

		// ========
		// BaseTask
		// ========

		class BaseTask {
		
		public:
			typedef std::function<bool(BaseTask&)> t_compareFunc;

			std::chrono::system_clock::time_point time;
			unsigned long delay;
			unsigned long repeat;
			unsigned long iteration;
			void* data;

			/*
			TODO implement our own linked list, taylored to our specific needs
			BaseTask* previous = 0;
			BaseTask* next = 0;
			Scheduler* scheduler = 0;
			*/

			BaseTask( std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ ) :
				time( time_ ),
				delay( delay_ ),
				repeat( repeat_ ),
				iteration( 0 ),
				data( data_ )
			{
			};
			virtual ~BaseTask() { };

			virtual void execute() = 0;
			virtual void complete() = 0;
			void proceed( unsigned long wait_ = 0 );
			void advance( unsigned long duration_ );

		}; // class BaseTask

		// ====
		// Task
		// ====

		template<typename T = void> class Task : public BaseTask {

		public:
			typedef std::function<T(Task<T>&)> t_taskFunc;

			Task( t_taskFunc&& func_, std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ ) :
				BaseTask( time_, delay_, repeat_, data_ ),
				m_func( std::move( func_ ) ),
				m_promise( std::make_shared<std::promise<T> >() ),
				m_future( std::shared_future<T>( m_promise->get_future() ) )
			{
			};
			~Task() { };

			// NOTE the void variant of execute is specialized in the cpp file. This is done because set_value cannot
			// accept a return value of void from this->m_func. The declaration is at the bottom of this file, below
			// the class defenition.
			void execute() {
				// When a task is first created, a promise and an accompanying shared future is created. The value of this
				// shared future should be kept alive until the next execution of the task.
				if ( ! this->m_first ) {
					std::lock_guard<std::mutex> lock( this->m_futureMutex );
					this->m_future = std::shared_future<T>( m_promise->get_future() );
				}
				this->m_promise->set_value( this->m_func( *this ) );
				this->m_promise = std::make_shared<std::promise<T> >();
				this->m_first = false;
			};

			void complete() {
				this->wait();
			};

			T wait() const {
				std::unique_lock<std::mutex> lock( this->m_futureMutex );
				std::shared_future<T> future = this->m_future;
				lock.unlock();
				return future.get();
			};

			bool waitFor( unsigned long wait_ ) const {
				std::unique_lock<std::mutex> lock( this->m_futureMutex );
				std::shared_future<T> future = this->m_future;
				lock.unlock();
				return future.wait_for( std::chrono::milliseconds( wait_ ) ) == std::future_status::ready;
			};

		private:
			t_taskFunc m_func;
			std::shared_ptr<std::promise<T> > m_promise;
			std::shared_future<T> m_future;
			mutable std::mutex m_futureMutex;
			volatile bool m_first = true;

		}; // class Task

		// ==============
		// TaskComparator
		// ==============



		Scheduler();
		~Scheduler();

		template<typename V = void> std::shared_ptr<Task<V> > schedule( unsigned long delay_, unsigned long repeat_, void* data_, typename Task<V>::t_taskFunc&& func_ ) {
			std::shared_ptr<Task<V> > task = std::make_shared<Task<V> >( std::forward<typename Task<V>::t_taskFunc>( func_ ), std::chrono::system_clock::now() + std::chrono::milliseconds( delay_ ), delay_, repeat_, data_ );
			Scheduler::ThreadPool::get().schedule( this, std::static_pointer_cast<BaseTask>( task ) );
			return task;
		};

		template<typename V = void> std::shared_ptr<Task<V> > schedule( unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task<V> > task_ ) {
			task_->delay = delay_;
			task_->repeat = repeat_;
			task_->time = std::chrono::system_clock::now() + std::chrono::milliseconds( delay_ );
			Scheduler::ThreadPool::get().schedule( this, task_ );
			return task_;
		};

		template<typename V = void> std::shared_ptr<Task<V> > schedule( std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_, typename Task<V>::t_taskFunc&& func_ ) {
			std::shared_ptr<Task<V> > task = std::make_shared<Task<V> >( std::forward<typename Task<V>::t_taskFunc>( func_ ), time_, delay_, repeat_, data_ );
			Scheduler::ThreadPool::get().schedule( this, task );
			return task;
		};

		template<typename V = void> std::shared_ptr<Task<V> > schedule( std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task<V> > task_ ) {
			task_->delay = delay_;
			task_->repeat = repeat_;
			task_->time = time_;
			Scheduler::ThreadPool::get().schedule( this, task_ );
			return task_;
		};

		void erase( BaseTask::t_compareFunc&& func_ );
		std::shared_ptr<BaseTask> first( BaseTask::t_compareFunc&& func_ ) const;
		void notify();

	private:

		// ==========
		// ThreadPool
		// ==========

		class ThreadPool final {

		public:
			typedef std::pair<Scheduler*, std::shared_ptr<BaseTask> > t_scheduledTask;

			/*
			TODO implement our own linked list, taylored to our specific needs
			BaseTask* first = 0;
			*/

			ThreadPool( const ThreadPool& ) = delete; // Do not copy!
			ThreadPool& operator=( const ThreadPool& ) = delete; // Do not copy-assign!

			void schedule( Scheduler* scheduler_, std::shared_ptr<BaseTask> task_ );
			void erase( Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ = []( BaseTask& ) -> bool { return true; } );
			std::shared_ptr<BaseTask> first( const Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ = []( BaseTask& ) -> bool { return true; } ) const;
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
			std::vector<t_scheduledTask> m_tasks;
			std::vector<t_scheduledTask> m_activeTasks;
			mutable std::mutex m_tasksMutex;

			std::vector<std::thread> m_threads;
			std::condition_variable m_continueCondition;
			mutable std::mutex m_conditionMutex;

			ThreadPool(); // private constructor
			~ThreadPool(); // private destructor

			void _loop( unsigned int index_ );
			void _notify( bool all_, std::function<void()>&& func_ );

		}; // class ThreadPool

		unsigned int m_id;

	}; // class Scheduler

	// Task template specialization for void type. Implementation is in the cpp file.
	template<> void Scheduler::Task<void>::execute();

}; // namespace micasa
