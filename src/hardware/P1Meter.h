#pragma once

#include "../Hardware.h"

namespace micasa {

	class P1Meter final : public Hardware {

	public:
		P1Meter( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~P1Meter() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };
	
	}; // class P1Meter

}; // namespace micasa
