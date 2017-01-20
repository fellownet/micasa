#pragma once

#include "../Hardware.h"

namespace micasa {

	class P1Meter final : public Hardware {

	public:
		P1Meter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~P1Meter() { };
		
		std::string getLabel() const throw() override { return "P1 Meter"; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
	
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) throw() override { return std::chrono::milliseconds( 1000 ); }
		
	}; // class P1Meter

}; // namespace micasa
