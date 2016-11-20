#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFaceBoard final : public Hardware {

	public:
		PiFaceBoard( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~PiFaceBoard() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	}; // class PiFaceBoard

}; // namespace micasa
