#pragma once

#include "../Hardware.h"

namespace micasa {

	class Internal final : public Hardware {

	public:
		Internal( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~Internal() { };
		
		std::string toString() const;

	}; // class SolarEdge

}; // namespace micasa
