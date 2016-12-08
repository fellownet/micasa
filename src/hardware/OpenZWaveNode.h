#pragma once

#ifdef WITH_OPENZWAVE

#include <list>

#include "../Hardware.h"
#include "OpenZWave.h"

#include "Notification.h"

namespace micasa {

	class OpenZWaveNode final : public Hardware {

		friend class OpenZWave;
		
	public:
		struct PendingUpdate {
			std::timed_mutex updateMutex;
			std::condition_variable condition;
			std::mutex conditionMutex;
			bool done;
			unsigned int source;
			PendingUpdate( unsigned int source_ ) : source( source_ ) { };
		}; // struct PendingUpdate
		
		OpenZWaveNode( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Hardware( id_, reference_, parent_, label_ ) { };
		~OpenZWaveNode() { };
		
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ );

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ ) { return std::chrono::milliseconds( 1000 ); }

	private:
		bool m_dead = false;
		std::map<unsigned long long, std::shared_ptr<PendingUpdate> > m_pendingUpdates;
		mutable std::mutex m_pendingUpdatesMutex;
		
		void _handleNotification( const ::OpenZWave::Notification* notification_ );
		void _processValue( const ::OpenZWave::ValueID& valueId_, unsigned int source_ );
		const bool _queuePendingUpdate( const unsigned long long valueId_, const unsigned int source_ );
		
	}; // class OpenZWaveNode

}; // namespace micasa

#endif // WITH_OPENZWAVE
