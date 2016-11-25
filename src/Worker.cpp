#include <iostream>

#include "Worker.h"

namespace micasa {

	Worker::~Worker() {
#ifdef _DEBUG
		assert( this->m_shutdown && "Worker instance should not be running when being destroyed." );
#endif // _DEBUG
	};
	
	void Worker::_begin() {
#ifdef _DEBUG
		assert( this->m_shutdown && "Worker instance should not be running when invoking begin-method." );
#endif // _DEBUG
		this->m_shutdown = false;
		this->m_worker = std::thread( [this]{
			do {
				std::unique_lock<std::mutex> workLock( this->m_workMutex );
				std::chrono::milliseconds wait = this->_work( ++this->m_iteration );
				workLock.unlock();
				
				std::unique_lock<std::mutex> shutdownLock( this->m_shutdownMutex );
				this->m_shutdownCondition.wait_for( shutdownLock, wait, [&]{ return this->m_shutdown; } );
			} while( ! this->m_shutdown );
		} );
	};

	void Worker::_retire() {
#ifdef _DEBUG
		assert( ! this->m_shutdown && "Worker instance should be running when invoking retire-method." );
#endif // _DEBUG
		std::unique_lock<std::mutex> shutdownLock( this->m_shutdownMutex );
		this->m_shutdown = true;
		shutdownLock.unlock();

		this->m_shutdownCondition.notify_all();
		if ( this->m_worker.joinable() ) {
			this->m_worker.join();
		}
	};

	void Worker::_synchronize( std::function<void()> func_ ) {
		std::unique_lock<std::mutex> workLock( this->m_workMutex );
		func_();
		workLock.unlock();
	};
	
	bool Worker::isRunning() const {
		return this->m_shutdown == false && this->m_worker.joinable();
	};
	
	void Worker::wakeUp() {
		this->m_shutdownCondition.notify_all();
	};
	
}; // namespace micasa
