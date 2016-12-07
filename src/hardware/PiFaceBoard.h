#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFaceBoard final : public Hardware {

	public:
		PiFaceBoard( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string name_ ) : Hardware( id_, reference_, parent_, name_ ) { };
		~PiFaceBoard() { };
		
		bool updateDevice( const Device::UpdateSource source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };

	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ ) { return std::chrono::milliseconds( 1000 ); }

	}; // class PiFaceBoard

}; // namespace micasa
