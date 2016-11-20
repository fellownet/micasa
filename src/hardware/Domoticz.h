#pragma once

#include "../Hardware.h"

namespace micasa {

	class Domoticz final : public Hardware {

	public:
		Domoticz( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~Domoticz() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	}; // class SolarEdge

}; // namespace micasa
