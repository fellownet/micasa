#pragma once

#include "../Hardware.h"

namespace micasa {

	class RFXCom final : public Hardware {

	public:
		RFXCom( std::string id_, std::map<std::string, std::string> settings_ ) : Hardware( id_, settings_ ) { };
		~RFXCom() { };
		
		std::string toString() const;

	}; // class RFXCom

}; // namespace micasa
