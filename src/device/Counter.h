#pragma once

#include "../Device.h"

namespace micasa {

	class Counter final : public Device {

	public:
		Counter( std::shared_ptr<Hardware> hardware_ ) : Device( hardware_ ) { };

		std::string toString() const;
		void updateValue( int newValue_ );

	}; // class Counter

}; // namespace micasa
