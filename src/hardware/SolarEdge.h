#pragma once

#include "../Hardware.h"

namespace micasa {

	class SolarEdge final : public Hardware {

	public:
		SolarEdge( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~SolarEdge() { };
		
		std::string toString() const;

	}; // class SolarEdge

}; // namespace micasa
