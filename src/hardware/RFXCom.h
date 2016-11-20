#pragma once

#include "../Hardware.h"

namespace micasa {

	class RFXCom final : public Hardware {

	public:
		RFXCom( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~RFXCom() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	}; // class RFXCom

}; // namespace micasa
