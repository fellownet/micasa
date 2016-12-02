#pragma once

#ifdef WITH_OPENZWAVE

#include "../Hardware.h"

#include "Notification.h"

namespace micasa {

	class OpenZWaveNode final : public Hardware {

		friend class OpenZWave;
		
	public:
		OpenZWaveNode( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~OpenZWaveNode() { };
		
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ );

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ ) { return std::chrono::milliseconds( 1000 ); }

	private:
		void _handleNotification( const ::OpenZWave::Notification* notification_ );
		void _processValue( const ::OpenZWave::ValueID& valueId_, const Device::UpdateSource& source_ );
		
	}; // class OpenZWaveNode

}; // namespace micasa

#endif // WITH_OPENZWAVE
