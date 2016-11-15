#pragma once

#include "../Hardware.h"

namespace micasa {

	class HarmonyHub final : public Hardware {

	public:
		HarmonyHub( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~HarmonyHub() { };
		
		std::string toString() const;

	}; // class HarmonyHub

}; // namespace micasa
