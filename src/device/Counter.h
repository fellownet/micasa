#pragma once

#include "../Device.h"

namespace micasa {

	class Counter final : public Device {

	public:
		Counter( std::shared_ptr<Hardware> hardware_, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ ) : Device( hardware_, id_, unit_, name_, settings_ ) { };

		std::string toString() const;
		void updateValue( int newValue_ );

	}; // class Counter

}; // namespace micasa
