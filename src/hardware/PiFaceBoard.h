#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFaceBoard final : public Hardware {

	public:
		PiFaceBoard( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~PiFaceBoard() { };
		
		std::string toString() const;

	}; // class PiFaceBoard

}; // namespace micasa
