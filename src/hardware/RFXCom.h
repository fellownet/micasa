#pragma once

#include "../Hardware.h"

namespace micasa {

	class RFXCom final : public Hardware {

	public:
		RFXCom( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~RFXCom() { };
		
		std::string getLabel() const throw() override { return "RFXCom"; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) throw() override { return std::chrono::milliseconds( 1000 ); }

	}; // class RFXCom

}; // namespace micasa
