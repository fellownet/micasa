#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFaceBoard final : public Hardware {

	public:
		PiFaceBoard( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~PiFaceBoard() { };
		
		std::string toString() const;

	}; // class PiFaceBoard

}; // namespace micasa
