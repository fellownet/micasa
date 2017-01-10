#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFace final : public Hardware {

	public:
		PiFace( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~PiFace() { };
		
		std::string getLabel() const throw() override { return "PiFace"; };
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) throw() override { return std::chrono::milliseconds( 1000 ); }

	}; // class PiFace

}; // namespace micasa
