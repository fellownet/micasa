#pragma once

#ifdef WITH_OPENZWAVE

#include <mutex>

#include "../Hardware.h"

#include "Manager.h"

#include "Notification.h"

namespace micasa {

	class OpenZWave final : public Hardware {
		
		friend class OpenZWaveNode;

	public:
		OpenZWave( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~OpenZWave() { };
		
		void start() override;
		void stop() override;
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

		void handleNotification( const ::OpenZWave::Notification* notification_ );

	protected:
		
	private:
		static std::mutex s_managerMutex;
		
	}; // class OpenZWave

}; // namespace micasa

#endif // WITH_OPENZWAVE
