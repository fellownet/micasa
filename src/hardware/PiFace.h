#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFace final : public Hardware {

	public:
		PiFace( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~PiFace() { };
		
		std::string toString() const;

	}; // class PiFace

}; // namespace micasa
