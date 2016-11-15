#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFace final : public Hardware {

	public:
		PiFace( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~PiFace() { };
		
		std::string toString() const;

	}; // class PiFace

}; // namespace micasa
