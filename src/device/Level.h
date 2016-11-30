#pragma once

#include "../Device.h"

namespace micasa {

	class Level final : public Device {

	public:
		enum Unit {
			TICKS = 1,
			PERCENT,
			WATT,
			VOLT,
			DEGREES,
			PASCAL
		}; // enum Unit

		Level( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ ) : Device( hardware_, id_, reference_, name_ ) { };
		
		void start() override;
		void stop() override;
		bool updateValue( const Device::UpdateSource source_, const float value_ );
		const float getValue() const { return this->m_value; };

		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		float m_value;

	}; // class Level

}; // namespace micasa
