#pragma once

#include "../Hardware.h"

namespace micasa {

	class Domoticz final : public Hardware {

	public:
		Domoticz( const unsigned int id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~Domoticz() { };
		
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ ) { return std::chrono::milliseconds( 1000 ); }

	}; // class SolarEdge

}; // namespace micasa
