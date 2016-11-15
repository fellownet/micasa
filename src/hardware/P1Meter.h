#pragma once

#include "../Hardware.h"

namespace micasa {

	class P1Meter final : public Hardware {

	public:
		P1Meter( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~P1Meter() { };
		
		std::string toString() const;

	}; // class P1Meter

}; // namespace micasa
