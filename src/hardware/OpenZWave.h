#pragma once

#ifdef WITH_OPENZWAVE

#include "../Hardware.h"

namespace micasa {

	class OpenZWave final : public Hardware {

	public:
		OpenZWave( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~OpenZWave() { };
		
		std::string toString() const;
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

	}; // class OpenZWave

}; // namespace micasa

#endif // WITH_OPENZWAVE
