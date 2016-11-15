#pragma once

#include <string>

#include "../Device.h"

namespace micasa {

	class Switch final : public Device {

	private:

	public:
		Switch( std::shared_ptr<Hardware> hardware_ ) : Device( hardware_ ) { };

		std::string toString() const;
		void updateValue( std::string newValue_ );

	}; // class Switch

}; // namespace micasa
