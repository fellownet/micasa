#pragma once

#include "../Hardware.h"

namespace micasa {

	class P1Meter final : public Hardware {

	public:
		P1Meter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Hardware( id_, type_, reference_, parent_, label_ ) { };
		~P1Meter() { };
		
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };
	
	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ ) { return std::chrono::milliseconds( 1000 ); }
		
	}; // class P1Meter

}; // namespace micasa
