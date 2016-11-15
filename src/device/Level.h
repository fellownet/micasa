#pragma once

#include "../Device.h"

namespace micasa {

	class Level final : public Device {

	private:

	public:
		Level( std::shared_ptr<Hardware> hardware_ ) : Device( hardware_ ) { };

		std::string toString() const;
		void updateValue( float newValue_ );
		void updateValue( int newValue_ );

	}; // class Level

}; // namespace micasa
