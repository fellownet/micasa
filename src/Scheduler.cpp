#include <iostream>
#include <algorithm>
#include <atomic>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Scheduler.h"

namespace micasa {

	using namespace std::chrono;

	// ========
	// BaseTask
	// ========

	void Scheduler::BaseTask::proceed( unsigned long wait_ ) {
		this->time = system_clock::now() + milliseconds( wait_ );
		Scheduler::ThreadPool::get().notify();
	};

	void Scheduler::BaseTask::advance( unsigned long duration_ ) {
		this->time -= milliseconds( duration_ );
		Scheduler::ThreadPool::get().notify();
	};

	// =======
	// Task<T>
	// =======

	template<> void Scheduler::Task<void>::execute() {
		if ( ! this->m_first ) {
			std::lock_guard<std::mutex> lock( this->m_futureMutex );
			this->m_future = std::shared_future<void>( m_promise->get_future() );
		}
		this->m_func( *this /*std::static_pointer_cast<Task<void> >( this->shared_from_this() )*/ );
		this->m_promise->set_value(); // reason for specialization of void variant
		this->m_promise = std::make_shared<std::promise<void> >();
		this->m_first = false;
	}

	// =========
	// Scheduler
	// =========

	Scheduler::Scheduler() {
		static std::atomic<unsigned int> nextId( 0 );
		this->m_id = ++nextId;
	};

	Scheduler::~Scheduler() {
		// Purging all tasks for this scheduler *blocks* the current thread until all running tasks are completed.
		auto& pool = Scheduler::ThreadPool::get();
		pool.erase( this );
	};

	void Scheduler::erase( BaseTask::t_compareFunc&& func_ ) {
		Scheduler::ThreadPool::get().erase( this, std::move( func_ ) );
	};

	std::shared_ptr<Scheduler::BaseTask> Scheduler::first( BaseTask::t_compareFunc&& func_ ) const {
		return Scheduler::ThreadPool::get().first( this, std::move( func_ ) );
	};

	void Scheduler::notify() {
		Scheduler::ThreadPool::get().notify();
	};

	// ==========
	// ThreadPool
	// ==========

	Scheduler::ThreadPool::ThreadPool() :
		m_shutdown( false ),
		m_continue( false ),
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
		assert( this->m_tasks.empty() && "All tasks should be purged when ThreadPool is destructed." );
		assert( this->m_activeTasks.size() == 0 && "All active tasks should be completed when ThreadPool is destructed.");
		tasksLock.unlock();
#endif // _DEBUG
	};

	void Scheduler::ThreadPool::schedule( Scheduler* scheduler_, std::shared_ptr<BaseTask> task_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		this->m_tasks.push_back( { scheduler_, task_ } );
		tasksLock.unlock();
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::erase( Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
		
		// Remove all pending tasks from the task queue.
		for ( auto taskIt = this->m_tasks.begin(); taskIt != this->m_tasks.end(); ) {
			if (
				taskIt->first == scheduler_
				&& func_( *( taskIt->second.get() ) )
			) {
				taskIt = this->m_tasks.erase( taskIt );
			} else {
				taskIt++;
			}
		}

		// After all pending tasks have been removed, all running tasks should be instructed *not* to repeat. Their
		// futures are gathered.
		std::vector<std::shared_ptr<BaseTask> > tasks;
		for ( auto taskIt = this->m_activeTasks.begin(); taskIt != this->m_activeTasks.end(); taskIt++ ) {
			if (
				taskIt->first == scheduler_
				&& func_( *( taskIt->second.get() ) )
			) {
				taskIt->second->repeat = 0;
				tasks.push_back( taskIt->second );
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
		for ( auto taskIt = this->m_tasks.begin(); taskIt != this->m_tasks.end(); taskIt++ ) {
			if (
				taskIt->first == scheduler_
				&& func_( *( taskIt->second.get() ) )
			) {
				return taskIt->second;
			}
		}
		return nullptr;
	};

	void Scheduler::ThreadPool::notify() {
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::_loop( unsigned int index_ ) {
		while( ! this->m_shutdown ) {
			std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );

			if ( ! this->m_tasks.empty() ) {
				std::nth_element( this->m_tasks.begin(), this->m_tasks.begin(), this->m_tasks.end(), []( const t_scheduledTask& a_, const t_scheduledTask& b_ ) -> bool {
					return a_.second->time < b_.second->time;
				} );
				if ( this->m_tasks.front().second->time <= system_clock::now() ) {
					
					// Pop the first scheduled task from the queue and inform other threads about the queue change.
					t_scheduledTask stask = this->m_tasks.front();
					this->m_tasks.front() = std::move( this->m_tasks.back() );
					this->m_tasks.pop_back();
					this->_notify( false, [this]() -> void { this->m_continue = true; } );

					// Push the task into the active tasks list.
					this->m_activeTasks.push_back( stask );
					tasksLock.unlock();
					
					stask.second->iteration++;
					stask.second->execute();

					tasksLock.lock();
					auto find = std::find( this->m_activeTasks.begin(), this->m_activeTasks.end(), stask );
					if ( find != this->m_activeTasks.end() ) {
						this->m_activeTasks.erase( find );
					}

					if ( stask.second->repeat > 1 ) {
						if ( stask.second->repeat != SCHEDULER_INFINITE ) {
							stask.second->repeat--;
						}
						auto now = system_clock::now();
						do {
							// Make sure the thread doesn't get saturated at the expense of skipping intervals.
							stask.second->time += milliseconds( stask.second->delay );
						} while( stask.second->time < now );
						this->m_tasks.push_back( stask );
						this->_notify( false, [this]() -> void { this->m_continue = true; } );
					}
				}
			}

			auto predicate = [&]() -> bool { return this->m_shutdown || this->m_continue; };
			std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
			if ( this->m_tasks.empty() ) {
				tasksLock.unlock();
				this->m_continueCondition.wait( conditionLock, predicate );
			} else {
				auto now = system_clock::now();
				if ( now < this->m_tasks.front().second->time ) {
					auto wait = this->m_tasks.front().second->time - now;
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
	
	/*
	void Scheduler::ThreadPool::_insert( Scheduler* scheduler_, std::shared_ptr<BaseTask> task_ ) {
		// NOTE this method should be called while holding a lock on the tasks mutex!
		auto taskIt = this->m_tasks.begin();
		while(
			  taskIt != this->m_tasks.end()
			  && task_->time > taskIt->second->time
		) {
			taskIt++;
		}
		this->m_tasks.insert( taskIt, { scheduler_, task_ } );
	};
	*/

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