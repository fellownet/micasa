#pragma once

#include "../Hardware.h"

namespace micasa {

	class P1Meter final : public Hardware {

	public:
		P1Meter( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~P1Meter() { };
		
		std::string toString() const;

	}; // class P1Meter

}; // namespace micasa
