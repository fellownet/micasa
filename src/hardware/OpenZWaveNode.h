#pragma once

#ifdef WITH_OPENZWAVE

#include "../Hardware.h"

namespace micasa {

	class OpenZWaveNode final : public Hardware {

	public:
		OpenZWaveNode( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~OpenZWaveNode() { };
		
		std::string toString() const;

	}; // class OpenZWaveNode

}; // namespace micasa

#endif // WITH_OPENZWAVE
