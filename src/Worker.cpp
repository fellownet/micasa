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
		this->m_worker = std::thread( [&]{
			do {
				std::chrono::milliseconds wait = this->_work( ++this->m_iteration );
				std::unique_lock<std::mutex> lock( this->m_shutdownMutex );
				this->m_shutdownCondition.wait_for( lock, wait, [&]{ return this->m_shutdown; } );
			} while( ! this->m_shutdown );
		} );
	};

	void Worker::_retire() {
#ifdef _DEBUG
		assert( ! this->m_shutdown && "Worker instance should be running when invoking retire-method." );
#endif // _DEBUG
		{
			std::lock_guard<std::mutex> lock( this->m_shutdownMutex );
			this->m_shutdown = true;
		}
		this->m_shutdownCondition.notify_all();
		if ( this->m_worker.joinable() ) {
			this->m_worker.join();
		}
	};

	bool Worker::isRunning() {
		return this->m_shutdown == false && this->m_worker.joinable();
	}
	
	void Worker::notify() {
		this->m_shutdownCondition.notify_all();
	}
	
}; // namespace micasa
