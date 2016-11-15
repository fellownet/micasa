#pragma once

#include "../Hardware.h"

namespace micasa {

	class HarmonyHub final : public Hardware {

	public:
		HarmonyHub( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~HarmonyHub() { };
		
		std::string toString() const;

	}; // class HarmonyHub

}; // namespace micasa
