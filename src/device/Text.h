#pragma once

#include <string>

#include "../Device.h"

namespace micasa {

	class Text final : public Device {

	private:

	public:
		Text( std::shared_ptr<Hardware> hardware_ ) : Device( hardware_ ) { };

		std::string toString() const;
		void updateValue( std::string newValue_ );

	}; // class Text

}; // namespace micasa
