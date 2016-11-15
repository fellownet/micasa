#pragma once

#include "../Hardware.h"

namespace micasa {

	class Domoticz final : public Hardware {

	public:
		Domoticz( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~Domoticz() { };
		
		std::string toString() const;

	}; // class SolarEdge

}; // namespace micasa
