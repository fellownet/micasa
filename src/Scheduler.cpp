#include <iostream>
#include <algorithm>
#include <atomic>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Scheduler.h"

namespace micasa {

	using namespace std::chrono;

	Scheduler::Task::Task( t_taskFunc&& func_, std::chrono::steady_clock::time_point time_, unsigned long delay_, unsigned long repeat_, void* data_ ) :
		time( time_ ),
		delay( delay_ ),
		repeat( repeat_ ),
		iteration( 0 ),
		data( data_ ),
		m_func( std::move( func_ ) ),
		m_promise( std::make_shared<std::promise<void> >() )
	{
	};

	void Scheduler::Task::proceed( unsigned long wait_ ) {
		this->time = steady_clock::now() + milliseconds( wait_ );
		this->m_scheduler->notify();
	};

	Scheduler::ThreadPool::ThreadPool() :
		m_shutdown( false ),
		m_continue( false ),
		m_threads( std::vector<std::thread>( std::max( 1U, std::thread::hardware_concurrency() ) ) )
	{
		for ( unsigned int i = 0; i < this->m_threads.size(); i++ ) {
			this->m_threads[i] = std::thread( [this,i]() { this->_threadLoop( i ); } );
		}
	};

	Scheduler::ThreadPool::~ThreadPool() {
#ifdef _DEBUG
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		assert( this->m_tasks.size() == 0 && "All tasks should be purged when ThreadPool is destructed." );
		assert( this->m_activeTasks.size() == 0 && "All active tasks should be completed when ThreadPool is destructed.");
		tasksLock.unlock();
#endif // _DEBUG
		this->_notify( true, [this]() -> void { this->m_shutdown = true; } );
		for ( auto& thread: this->m_threads ) {
			thread.join();
		}
	};

	void Scheduler::ThreadPool::addTask( Scheduler* scheduler_, std::shared_ptr<Task> task_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		task_->m_scheduler = scheduler_;
		this->m_tasks.push( task_ );
		tasksLock.unlock();
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::purgeTasks( Scheduler* scheduler_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		
		// A priority queue doesn't support iterating directly, so iterating is implemented rather inefficiently here.
		// This is accepted here though, because it is likely that schedulers are not created and destroyed very often.
		std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task> >, TaskComparator> tasks;
		while( ! this->m_tasks.empty() ) {
			std::shared_ptr<Task> task = this->m_tasks.top(); //const_cast<std::shared_ptr<Task> >( this->m_tasks.top() );
			this->m_tasks.pop();
			if ( task->m_scheduler != scheduler_ ) {
				tasks.push( task );
			}
		}
		this->m_tasks = std::move( tasks );

		// After all pending tasks have been removed, all running tasks should be instructed *not* to repeat.
		std::vector<std::shared_ptr<std::promise<void> > > promises;
		for ( auto taskIt = this->m_activeTasks[scheduler_].begin(); taskIt != this->m_activeTasks[scheduler_].end(); taskIt++ ) {
			(*taskIt)->repeat = 0;
			promises.push_back( (*taskIt)->m_promise );
		}
		tasksLock.unlock();
		for ( auto const &promise : promises ) {
			promise->get_future().get();
		}
#ifdef _DEBUG
		tasksLock.lock();
		assert( this->m_activeTasks[scheduler_].size() == 0 && "All tasks should be completed after purge.");
		tasksLock.unlock();
#endif // _DEBUG
		this->m_activeTasks.erase( scheduler_ );
	};

	void Scheduler::ThreadPool::notify() {
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::_threadLoop( unsigned int index_ ) {
		while( ! this->m_shutdown ) {
			std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
			while(
				! this->m_tasks.empty()
				&& this->m_tasks.top()->time <= steady_clock::now()
			) {
				std::shared_ptr<Task> task = this->m_tasks.top(); //const_cast<std::shared_ptr<Task> >( this->m_tasks.top() );
				this->m_tasks.pop();
				this->_notify( false, [this]() -> void { this->m_continue = true; } );
				auto& activeTasks = this->m_activeTasks[task->m_scheduler];
				activeTasks.push_back( task );
				task->iteration++;
				tasksLock.unlock();
				
				task->m_func( task );
				task->m_promise->set_value();

				tasksLock.lock();
				auto find = std::find( activeTasks.begin(), activeTasks.end(), task );
				if ( find != activeTasks.end() ) {
					activeTasks.erase( find );
				}

				if ( task->repeat > 1 ) {
					if ( task->repeat != SCHEDULER_INFINITE ) {
						task->repeat--;
					}
					task->time += milliseconds( task->delay );
					task->m_promise = std::make_shared<std::promise<void> >();
					this->m_tasks.push( task );
					this->_notify( false, [this]() -> void { this->m_continue = true; } );
				}
			}

			auto predicate = [&]() -> bool { return this->m_shutdown || this->m_continue; };
			std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
			if ( this->m_tasks.empty() ) {
				tasksLock.unlock();
				this->m_continueCondition.wait( conditionLock, predicate );
			} else {
				auto now = steady_clock::now();
				if ( now < this->m_tasks.top()->time ) {
					auto wait = this->m_tasks.top()->time - now;
					tasksLock.unlock();
					this->m_continueCondition.wait_for( conditionLock, wait, predicate );
				} else {
					tasksLock.unlock();
				}
			}
			this->m_continue = false;
			conditionLock.unlock();
		}
	};

	void Scheduler::ThreadPool::_notify( bool bAll_, std::function<void()>&& func_ ) {
		std::lock_guard<std::mutex> lock( this->m_conditionMutex );
		func_();
		if ( bAll_ ) {
			this->m_continueCondition.notify_all();
		} else {
			this->m_continueCondition.notify_one();
		}
	};

	Scheduler::Scheduler() {
		static std::atomic<unsigned int> nextId( 0 );
		this->m_id = ++nextId;
	};

	Scheduler::~Scheduler() {
		// Purging all tasks for this scheduler *blocks* the current thread until all running tasks are completed.
		auto& pool = Scheduler::ThreadPool::get();
		pool.purgeTasks( this );
	};

	std::shared_ptr<Scheduler::Task> Scheduler::schedule( unsigned long delay_, unsigned long repeat_, void* data_, Scheduler::Task::t_taskFunc&& func_ ) {
		std::shared_ptr<Scheduler::Task> task = std::make_shared<Scheduler::Task>( std::forward<Scheduler::Task::t_taskFunc>( func_ ), steady_clock::now() + milliseconds( delay_ ), delay_, repeat_, data_ );
		Scheduler::ThreadPool::get().addTask( this, task );
		return task;
	};

	std::shared_ptr<Scheduler::Task> Scheduler::schedule( unsigned long delay_, unsigned long repeat_, std::shared_ptr<Task> task_ ) {
		task_->delay = delay_;
		task_->repeat = repeat_;
		task_->time = steady_clock::now() + milliseconds( delay_ );
		Scheduler::ThreadPool::get().addTask( this, task_ );
		return task_;
	};

	std::shared_ptr<Scheduler::Task> Scheduler::schedule( std::chrono::steady_clock::time_point time_, void* data_, Task::t_taskFunc&& func_ ) {
		std::shared_ptr<Scheduler::Task> task = std::make_shared<Scheduler::Task>( std::forward<Scheduler::Task::t_taskFunc>( func_ ), time_, 0, 1, data_ );
		Scheduler::ThreadPool::get().addTask( this, task );
		return task;
	};
	
	std::shared_ptr<Scheduler::Task> Scheduler::schedule( std::chrono::steady_clock::time_point time_, std::shared_ptr<Task> task_ ) {
		task_->delay = 0;
		task_->repeat = 1;
		task_->time = time_;
		Scheduler::ThreadPool::get().addTask( this, task_ );
		return task_;
	};

	void Scheduler::notify() {
		Scheduler::ThreadPool::get().notify();
	};

}; // namespace micasa