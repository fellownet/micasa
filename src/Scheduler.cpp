#include <iostream>
#include <algorithm>
#include <atomic>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Scheduler.h"

namespace micasa {

	using namespace std::chrono;

	// =========
	// Scheduler
	// =========

	Scheduler::~Scheduler() {
		Scheduler::ThreadPool::get().erase( this );
	};

	void Scheduler::erase( BaseTask::t_compareFunc&& func_ ) {
		Scheduler::ThreadPool::get().erase( this, std::move( func_ ) );
	};

	std::shared_ptr<Scheduler::BaseTask> Scheduler::first( BaseTask::t_compareFunc&& func_ ) const {
		return Scheduler::ThreadPool::get().first( this, std::move( func_ ) );
	};

	void Scheduler::proceed( unsigned long wait_, std::shared_ptr<BaseTask> task_ ) {
		Scheduler::ThreadPool::get().proceed( this, wait_, task_ );
	};

	// ==========
	// ThreadPool
	// ==========

	Scheduler::ThreadPool::ThreadPool() :
		m_shutdown( false ),
		m_continue( false ),
		m_count( 0 ),
		m_threads( std::vector<std::thread>( std::max( 2U, 2 * std::thread::hardware_concurrency() ) ) )
	{
		for ( unsigned int i = 0; i < this->m_threads.size(); i++ ) {
			this->m_threads[i] = std::thread( [this,i]() { this->_loop( i ); } );
		}
	};

	Scheduler::ThreadPool::~ThreadPool() {
		this->_notify( true, [this]() -> void { this->m_shutdown = true; } );
		for ( auto& thread: this->m_threads ) {
			thread.join();
		}
#ifdef _DEBUG
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		assert( this->m_start == nullptr && "All tasks should be purged when ThreadPool is destructed." );
		assert( this->m_activeTasks.size() == 0 && "All active tasks should be completed when ThreadPool is destructed." );
		tasksLock.unlock();
#endif // _DEBUG
	};

	void Scheduler::ThreadPool::schedule( std::shared_ptr<BaseTask> task_ ) {
#ifdef _DEBUG
		assert( this->m_shutdown == false && "Tasks should only be scheduled when scheduler is running." );
#endif // _DEBUG
		std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
		this->_insert( task_ );
	};

	void Scheduler::ThreadPool::erase( Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		
		// The linked list is circular and because it's modified while iterating we're cannot detect when the invariant
		// is looping. Instead, we're going to just investigate as many of the tasks as there are in the task queue and
		// remove them if the condition matches.
		auto position = this->m_start;
		for ( unsigned int i = 0, total = this->m_count; i < total; i++ ) {
			auto task = position;
			position = position->m_next;
			if (
				task->m_scheduler == scheduler_
				&& func_( *( task.get() ) )
			) {
				this->_erase( task );
			}
		}

		// After all pending tasks have been removed, all running tasks should be instructed *not* to repeat. Their
		// futures are gathered.
		std::vector<std::shared_ptr<BaseTask> > tasks;
		for ( auto taskIt = this->m_activeTasks.begin(); taskIt != this->m_activeTasks.end(); taskIt++ ) {
			if (
				(*taskIt)->m_scheduler == scheduler_
				&& func_( *( taskIt->get() ) )
			) {
				(*taskIt)->repeat = 0;
				tasks.push_back( *taskIt );
			}
		}

		tasksLock.unlock();

		// Block the thread until all tasks have been completed.
		for ( auto const &task : tasks ) {
			task->complete();
		}
	};

	std::shared_ptr<Scheduler::BaseTask> Scheduler::ThreadPool::first( const Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ ) const {
		std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
		auto position = this->m_start;
		do {
			if (
				position->m_scheduler == scheduler_
				&& func_( *( position.get() ) )
			) {
				return position;
			}
			position = position->m_next;
		} while( position != this->m_start );
		return nullptr;
	};
	
	void Scheduler::ThreadPool::proceed( const Scheduler* scheduler_, unsigned long wait_, std::shared_ptr<BaseTask> task_ ) {
		std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
		this->_erase( task_ );
		task_->time = system_clock::now() + milliseconds( wait_ );
		this->_insert( task_ );
	};

	void Scheduler::ThreadPool::_loop( unsigned int index_ ) {
		while( ! this->m_shutdown ) {
			std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
			if (
				this->m_start != nullptr
				&& this->m_start->time <= system_clock::now()
			) {
				auto task = this->m_start;
				this->_erase( task );
				this->m_activeTasks.push_back( task );
				tasksLock.unlock();
				
				task->iteration++;
				task->execute();

				tasksLock.lock();
				auto find = std::find( this->m_activeTasks.begin(), this->m_activeTasks.end(), task );
				if ( find != this->m_activeTasks.end() ) {
					this->m_activeTasks.erase( find );
				}

				if ( task->repeat > 1 ) {
					if ( task->repeat != SCHEDULER_INFINITE ) {
						task->repeat--;
					}
					auto now = system_clock::now();
					do {
						// Make sure the thread doesn't get saturated at the expense of skipping intervals.
						task->time += milliseconds( task->delay );
					} while( task->time < now );
					this->_insert( task );
				}
			}

			auto predicate = [&]() -> bool { return this->m_shutdown || this->m_continue; };
			std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
			if ( this->m_start == nullptr ) {
				tasksLock.unlock();
				this->m_continueCondition.wait( conditionLock, predicate );
			} else {
				auto now = system_clock::now();
				if ( now < this->m_start->time ) {
					auto wait = this->m_start->time - now;
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
	
	void Scheduler::ThreadPool::_insert( std::shared_ptr<BaseTask> task_ ) {
#ifdef _DEBUG
		assert( task_->m_previous == nullptr && task_->m_next == nullptr && "Task should not be part of the linked list before being inserted to it." );
#endif // _DEBUG
		if ( this->m_start == nullptr ) {
			this->m_start = task_;
			task_->m_previous = task_->m_next = task_;
		} else {
			auto position = this->m_start;
			while( position->time < task_->time ) {
				position = position->m_next;
				if ( position == this->m_start ) {
					break;
				}
			}
			task_->m_previous = position->m_previous;
			task_->m_next = position;
			position->m_previous->m_next = task_;
			position->m_previous = task_;

			if ( task_->time < this->m_start->time ) {
				this->m_start = task_;
			}
#ifdef _DEBUG
		assert( ( task_->m_next == this->m_start || task_->m_next->time >= task_->time ) && "Linked list of tasks should be sorted acending." );
		assert( this->m_start->time <= this->m_start->m_next->time && "Start should be the first task in the list." );
#endif // _DEBUG
		}
		this->m_count++;
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::_erase( std::shared_ptr<BaseTask> task_ ) {
#ifdef _DEBUG
		assert( task_->m_previous != nullptr && task_->m_next != nullptr && "Task should be part of the linked list before being removed from it." );
		assert( ( task_->m_next == this->m_start || task_->m_next->time >= task_->time ) && "Linked list of tasks should be sorted acending." );
#endif // _DEBUG
		if ( task_->m_next == task_ ) {
			this->m_start = nullptr;
		} else {
			if ( this->m_start == task_ ) {
				this->m_start = task_->m_next;
			}
			task_->m_previous->m_next = task_->m_next;
			task_->m_next->m_previous = task_->m_previous;
		}
		task_->m_previous = task_->m_next = nullptr;
#ifdef _DEBUG
		assert( ( this->m_start == nullptr || this->m_start->time <= this->m_start->m_next->time ) && "Start should be the first task in the list." );
#endif // _DEBUG
		this->m_count--;
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::_notify( bool all_, std::function<void()>&& func_ ) {
		std::lock_guard<std::mutex> lock( this->m_conditionMutex );
		func_();
		if ( all_ ) {
			this->m_continueCondition.notify_all();
		} else {
			this->m_continueCondition.notify_one();
		}
	};

}; // namespace micasa