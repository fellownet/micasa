#pragma once

#include "../Hardware.h"

namespace micasa {

	class Internal final : public Hardware {

	public:
		Internal( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~Internal() { };
		
		std::string toString() const;

	}; // class SolarEdge

}; // namespace micasa
