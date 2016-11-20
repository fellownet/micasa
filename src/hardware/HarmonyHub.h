#pragma once

#include "../Hardware.h"

namespace micasa {

	class HarmonyHub final : public Hardware {

	public:
		HarmonyHub( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ) { };
		~HarmonyHub() { };
		
		std::string toString() const;
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	}; // class HarmonyHub

}; // namespace micasa
