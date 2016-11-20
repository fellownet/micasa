#pragma once

#ifdef WITH_OPENZWAVE

#include "../Hardware.h"

namespace micasa {

	class OpenZWaveNode final : public Hardware {

	public:
		OpenZWaveNode( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~OpenZWaveNode() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	}; // class OpenZWaveNode

}; // namespace micasa

#endif // WITH_OPENZWAVE
