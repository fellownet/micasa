#include <iostream>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Worker.h"

namespace micasa {

	Worker::~Worker() {
#ifdef _DEBUG
		assert( this->m_shutdown && "Worker instance should not be running when being destroyed." );
#endif // _DEBUG
	};
	
	void Worker::start() {
#ifdef _DEBUG
		assert( this->m_shutdown && "Worker instance should not be running when invoking begin-method." );
#endif // _DEBUG
		this->m_shutdown = false;
		this->m_worker = std::thread( [this] {
			do {
				std::unique_lock<std::mutex> workLock( this->m_workMutex );
				std::chrono::milliseconds wait = this->_work( ++this->m_iteration );
				workLock.unlock();
				
				std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
				this->m_continueCondition.wait_for( conditionLock, wait, [&]{ return this->m_shutdown || this->m_hasWork; } );
				this->m_hasWork = false;
				conditionLock.unlock(); // after the wait we own the lock
			} while( ! this->m_shutdown );
		} );
	};

	void Worker::stop() {
#ifdef _DEBUG
		assert( ! this->m_shutdown && "Worker instance should be running when invoking retire-method." );
#endif // _DEBUG
		std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
		this->m_shutdown = true;
		conditionLock.unlock();

		this->m_continueCondition.notify_all();
		if ( this->m_worker.joinable() ) {
			this->m_worker.join();
		}
	};

	void Worker::_synchronize( std::function<void()> func_ ) const {
		std::unique_lock<std::mutex> workLock( this->m_workMutex );
		func_();
		workLock.unlock();
	};
	
	bool Worker::isRunning() const {
		return this->m_shutdown == false && this->m_worker.joinable();
	};
	
	void Worker::wakeUp() {
		std::unique_lock<std::mutex> conditionLock( this->m_conditionMutex );
		this->m_hasWork = true;
		conditionLock.unlock();
		this->m_continueCondition.notify_all();
		
	};
	
}; // namespace micasa
