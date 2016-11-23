#pragma once

#include "../Hardware.h"

namespace micasa {

	class Internal final : public Hardware {

	public:
		Internal( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~Internal() { };
		
		std::string toString() const;
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

	}; // class SolarEdge

}; // namespace micasa
