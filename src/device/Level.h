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

		typedef double t_value;
		
		Level( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };
		const Device::Type getType() const { return Device::Type::LEVEL; };
		
		void start() override;
		void stop() override;
		bool updateValue( const unsigned int& source_, const t_value& value_ );
		const t_value& getValue() const { return this->m_value; };

		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );

	private:
		t_value m_value;

	}; // class Level

}; // namespace micasa
