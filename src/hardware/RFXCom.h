#pragma once

#include "../Hardware.h"

namespace micasa {

	class RFXCom final : public Hardware {

	public:
		RFXCom( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Hardware( id_, unit_, name_, settings_ ) { };
		~RFXCom() { };
		
		std::string toString() const;

	}; // class RFXCom

}; // namespace micasa
