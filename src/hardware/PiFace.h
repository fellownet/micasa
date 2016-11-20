#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFace final : public Hardware {

	public:
		PiFace( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~PiFace() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	}; // class PiFace

}; // namespace micasa
