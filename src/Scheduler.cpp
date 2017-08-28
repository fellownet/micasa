#include <iostream>
#include <algorithm>
#include <atomic>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Scheduler.h"
#include "Utils.h"

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
		assert( this->m_tasks.size() == 0 && "All tasks should be purged when ThreadPool is destructed." );
		assert( this->m_activeTasks.size() == 0 && "All active tasks should be completed when ThreadPool is destructed." );
		tasksLock.unlock();
#endif // _DEBUG
	};

	void Scheduler::ThreadPool::schedule( std::shared_ptr<BaseTask> task_ ) {
#ifdef _DEBUG
		assert( this->m_shutdown == false && "Tasks should only be scheduled when scheduler is running." );
#endif // _DEBUG
		std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
		this->m_tasks.insert( { task_->time, task_ } );
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::erase( Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ ) {
		std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );

		for ( auto taskIt = this->m_tasks.begin(); taskIt != this->m_tasks.end(); ) {
			if (
				taskIt->second->m_scheduler == scheduler_
				&& func_( *( taskIt->second.get() ) )
			) {
				taskIt = this->m_tasks.erase( taskIt );
			} else {
				taskIt++;
			}
		}

		std::vector<std::shared_ptr<BaseTask>> tasks;
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

		for ( auto const &task : tasks ) {
			task->complete();
		}
	};

	auto Scheduler::ThreadPool::first( const Scheduler* scheduler_, BaseTask::t_compareFunc&& func_ ) const -> std::shared_ptr<BaseTask> {
		std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
		for ( auto& taskIt : this->m_tasks ) {
			if (
				taskIt.second->m_scheduler == scheduler_
				&& func_( *( taskIt.second.get() ) )
			) {
				return taskIt.second;
			}
		}
		return nullptr;
	};

	void Scheduler::ThreadPool::proceed( const Scheduler* scheduler_, unsigned long wait_, std::shared_ptr<BaseTask> task_ ) {
		std::lock_guard<std::mutex> tasksLock( this->m_tasksMutex );
		for ( auto taskIt = this->m_tasks.begin(); taskIt != this->m_tasks.end(); ) {
			if (
				taskIt->second->m_scheduler == scheduler_
				&& taskIt->second == task_
			) {
				taskIt = this->m_tasks.erase( taskIt );
			} else {
				taskIt++;
			}
		}
		task_->time = system_clock::now() + milliseconds( wait_ );
		this->m_tasks.insert( { task_->time, task_ } );
		this->_notify( false, [this]() -> void { this->m_continue = true; } );
	};

	void Scheduler::ThreadPool::_loop( unsigned int index_ ) {
		while( ! this->m_shutdown ) {
			std::unique_lock<std::mutex> tasksLock( this->m_tasksMutex );
			if (
				this->m_tasks.size() > 0
				&& this->m_tasks.begin()->second->time <= system_clock::now()
			) {
				auto task = this->m_tasks.begin()->second;
				this->m_tasks.erase( this->m_tasks.begin() );
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
					if ( task->repeat != SCHEDULER_REPEAT_INFINITE ) {
						task->repeat--;
					}
					auto now = system_clock::now();
					do {
						task->time += milliseconds( task->delay );
					} while( task->time < now );
					this->m_tasks.insert( { task->time, task } );
					this->_notify( false, [this]() -> void { this->m_continue = true; } );
				}
			}

			auto predicate = [&]() -> bool { return this->m_shutdown || this->m_continue; };
			std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
			if ( __unlikely( this->m_tasks.size() == 0 ) ) {
				tasksLock.unlock();
				this->m_continueCondition.wait( conditionLock, predicate );
			} else {
				auto now = system_clock::now();
				if ( now < this->m_tasks.begin()->second->time ) {
					auto wait = this->m_tasks.begin()->second->time - now;
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

	inline void Scheduler::ThreadPool::_notify( bool all_, std::function<void()>&& func_ ) {
		std::lock_guard<std::mutex> lock( this->m_conditionMutex );
		func_();
		if ( __unlikely( all_ ) ) {
			this->m_continueCondition.notify_all();
		} else {
			this->m_continueCondition.notify_one();
		}
	};

}; // namespace micasa
