#pragma once

#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <climits>

#define SCHEDULER_INFINITE ULONG_MAX
#define SCHEDULER_INTERVAL_1SEC 1000
#define SCHEDULER_INTERVAL_3SEC 1000 * 3
#define SCHEDULER_INTERVAL_5SEC 1000 * 5
#define SCHEDULER_INTERVAL_10SEC 1000 * 10
#define SCHEDULER_INTERVAL_30SEC 1000 * 30
#define SCHEDULER_INTERVAL_1MIN 1000 * 60
#define SCHEDULER_INTERVAL_5MIN 1000 * 60 * 5
#define SCHEDULER_INTERVAL_HOUR 1000 * 60 * 60

namespace micasa {

	// =========
	// Scheduler
	// =========

	class Scheduler final {

		class ThreadPool; // forward declaration for friend clause of BaseTask

	public:

		// ========
		// BaseTask
		// ========

		class BaseTask: public std::enable_shared_from_this<BaseTask> {
			friend class ThreadPool;

		public:
			typedef std::function<bool(const BaseTask&)> t_compareFunc;

			BaseTask( const BaseTask& ) = delete; // Do not copy!
			BaseTask& operator=( const BaseTask& ) = delete; // Do not copy-assign!

			std::chrono::system_clock::time_point time;
			unsigned long delay;
			unsigned long repeat;
			unsigned long iteration;
			void* data;

			BaseTask( Scheduler* scheduler_, std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ ) :
				time( time_ ),
				delay( delay_ ),
				repeat( repeat_ ),
				iteration( 0 ),
				data( data_ ),
				m_scheduler( scheduler_ )
			{
			};
			virtual ~BaseTask() { };

			virtual void execute() = 0;
			virtual void complete() = 0;

		private:
			Scheduler* m_scheduler;
			std::shared_ptr<BaseTask> m_previous;
			std::shared_ptr<BaseTask> m_next;

		}; // class BaseTask

		// ====
		// Task
		// NOTE Specialization for void is at the bottom of this file.
		// ====

		template<typename T = void> class Task : public BaseTask {

		public:
			typedef std::function<T(std::shared_ptr<Task<T>>)> t_taskFunc;

			Task( Scheduler* scheduler_, t_taskFunc&& func_, std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ ) :
				BaseTask( scheduler_, time_, delay_, repeat_, data_ ),
				m_func( std::move( func_ ) ),
				m_first( true )
			{
				// Retrieving the first result for the task is blocking. All subsequent calls to retrieve the result are
				// instant, returning the last generated value.
				this->m_resultMutex.lock();
			};
			~Task() { };

			void execute() {
				if ( ! this->m_first ) {
					this->m_resultMutex.lock();
				}
				this->m_result = this->m_func( std::static_pointer_cast<Task<T>>( this->shared_from_this() ) );
				this->m_first = false;
				this->m_resultMutex.unlock();
			};

			void complete() {
				this->wait();
			};

			T wait() const {
				std::lock_guard<std::timed_mutex> lock( this->m_resultMutex );
				return this->m_result;
			};

			bool waitFor( unsigned long wait_ ) const {
				if ( this->m_resultMutex.try_lock_for( std::chrono::milliseconds( wait_ ) ) ) {
					this->m_resultMutex.unlock();
					return true;
				}
				return false;
			};

		private:
			t_taskFunc m_func;
			mutable std::timed_mutex m_resultMutex;
			T m_result;
			bool m_first;

		}; // class Task

		Scheduler() { };
		~Scheduler();

		Scheduler( const Scheduler& ) = delete; // do not copy
		Scheduler& operator=( const Scheduler& ) = delete; // do not copy-assign
		Scheduler( const Scheduler&& ) = delete; // do not move
		Scheduler& operator=( Scheduler&& ) = delete; // do not move-assign

		template<typename V = void> std::shared_ptr<Task<V>> schedule( unsigned long delay_, unsigned long repeat_, void* data_, typename Task<V>::t_taskFunc&& func_ ) {
			std::shared_ptr<Task<V>> task = std::make_shared<Task<V>>( this, std::move( func_ ), std::chrono::system_clock::now() + std::chrono::milliseconds( delay_ ), delay_, repeat_, data_ );
			Scheduler::ThreadPool::get().schedule( std::static_pointer_cast<BaseTask>( task ) );
			return task;
		};

		template<typename V = void> std::shared_ptr<Task<V>> schedule( std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_, typename Task<V>::t_taskFunc&& func_ ) {
			std::shared_ptr<Task<V>> task = std::make_shared<Task<V>>( this, std::move( func_ ), time_, delay_, repeat_, data_ );
			Scheduler::ThreadPool::get().schedule( task );
			return task;
		};

		template<typename V = void> std::shared_ptr<Task<V>> schedule( unsigned long wait_, unsigned long delay_, unsigned long repeat_, void* data_, typename Task<V>::t_taskFunc&& func_ ) {
			return this->schedule<V>( std::chrono::system_clock::now() + std::chrono::milliseconds( wait_ ), delay_, repeat_, data_, std::move( func_ ) );
		};

		template<typename V = void> std::shared_ptr<Task<V>> schedule( unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task<V>> task_ ) {
			task_->time = std::chrono::system_clock::now() + std::chrono::milliseconds( delay_ );
			task_->delay = delay_;
			task_->repeat = repeat_;
			Scheduler::ThreadPool::get().schedule( task_ );
			return task_;
		};

		template<typename V = void> std::shared_ptr<Task<V>> schedule( std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task<V>> task_ ) {
			task_->time = time_;
			task_->delay = delay_;
			task_->repeat = repeat_;
			Scheduler::ThreadPool::get().schedule( task_ );
			return task_;
		};

		template<typename V = void> std::shared_ptr<Task<V>> schedule( unsigned long wait_, unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task<V>> task_ ) {
			return this->schedule<V>( std::chrono::system_clock::now() + std::chrono::milliseconds( wait_ ), delay_, repeat_, task_ );
		};

		void erase( BaseTask::t_compareFunc&& func_ = []( const BaseTask& ) -> bool { return true; } );
		std::shared_ptr<BaseTask> first( BaseTask::t_compareFunc&& func_ = []( const BaseTask& ) -> bool { return true; } ) const;
		void proceed( unsigned long wait_, std::shared_ptr<BaseTask> task_ );
#ifdef _DEBUG
		static unsigned int count();
#endif // _DEBUG

	private:

		// ==========
		// ThreadPool
		// ==========

		class ThreadPool final {

		public:
			~ThreadPool(); // public destructor

			ThreadPool( const ThreadPool& ) = delete; // do not copy
			ThreadPool& operator=( const ThreadPool& ) = delete; // do not copy-assign
			ThreadPool( const ThreadPool&& ) = delete; // do not move
			ThreadPool& operator=( ThreadPool&& ) = delete; // do not move-assign

			void schedule( std::shared_ptr<BaseTask> task_ );
			void erase( Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ = []( const BaseTask& ) -> bool { return true; } );
			std::shared_ptr<BaseTask> first( const Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ = []( const BaseTask& ) -> bool { return true; } ) const;
			void proceed( const Scheduler* scheduler_, unsigned long wait_, std::shared_ptr<BaseTask> task_ );
#ifdef _DEBUG
			unsigned int count() const;
#endif // _DEBUG

			static ThreadPool& get() {
				// In c++11 static initialization is supposed to be thread-safe.
				static ThreadPool instance;
				return instance;
			}

		private:
			bool m_shutdown;
			bool m_continue;
			std::shared_ptr<BaseTask> m_start;
			unsigned int m_count;
			std::vector<std::shared_ptr<BaseTask>> m_activeTasks;
			mutable std::mutex m_tasksMutex;
			std::vector<std::thread> m_threads;
			std::condition_variable m_continueCondition;
			mutable std::mutex m_conditionMutex;

			ThreadPool(); // private constructor

			void _loop( unsigned int index_ );
			void _insert( std::shared_ptr<BaseTask> task_ );
			void _erase( std::shared_ptr<BaseTask> task_ );
			void _notify( bool all_, std::function<void()>&& func_ );

		}; // class ThreadPool

	}; // class Scheduler

	// ====
	// Task<void>
	// ====

	template<> class Scheduler::Task<void> : public BaseTask {

	public:
		typedef std::function<void(std::shared_ptr<Task<void>>)> t_taskFunc;

		Task( Scheduler* scheduler_, t_taskFunc&& func_, std::chrono::system_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ ) :
			BaseTask( scheduler_, time_, delay_, repeat_, data_ ),
			m_func( std::move( func_ ) ),
			m_first( true )
		{
			this->m_resultMutex.lock();
		};
		~Task() { };

		void execute() {
			if ( ! this->m_first ) {
				this->m_resultMutex.lock();
			}
			this->m_func( std::static_pointer_cast<Task<void>>( this->shared_from_this() ) );
			this->m_first = false;
			this->m_resultMutex.unlock();
		};

		void complete() {
			this->wait();
		};

		void wait() const {
			std::lock_guard<std::timed_mutex> lock( this->m_resultMutex );
		};

		bool waitFor( unsigned long wait_ ) const {
			return this->m_resultMutex.try_lock_for( std::chrono::milliseconds( wait_ ) );
		};

	private:
		t_taskFunc m_func;
		mutable std::timed_mutex m_resultMutex;
		bool m_first;

	}; // class Task<void>

}; // namespace micasa
