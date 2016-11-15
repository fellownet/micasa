#pragma once

#ifdef WITH_OPENZWAVE

#include "../Hardware.h"

namespace micasa {

	class OpenZWave final : public Hardware {

	public:
		OpenZWave( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~OpenZWave() { };
		
		std::string toString() const;

	}; // class OpenZWave

}; // namespace micasa

#endif // WITH_OPENZWAVE
