#pragma once

#include "../Device.h"

namespace micasa {

	class Level final : public Device {

	public:
		enum Unit {
			GENERIC = 1,
			PERCENT,
			WATT,
			VOLT,
			AMPERES,
			POWER_FACTOR,
			CELCIUS,
			FAHRENHEIT,
			PASCAL,
			LUX
		}; // enum Unit
		static const std::map<Unit, std::string> UnitText;

		typedef double t_value;
		static const Device::Type type = Device::Type::LEVEL;
		
		Level( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };

		Device::Type getType() const throw() override { return Level::type; };
		
		void start() override;
		void stop() override;
		bool updateValue( const unsigned int& source_, const t_value& value_ );
		t_value getValue() const throw() { return this->m_value; };
		t_value getPreviousValue() const throw() { return this->m_previousValue; };
		json getJson( bool full_ = false ) const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

	private:
		t_value m_value = 0;
		t_value m_previousValue = 0;

	}; // class Level

}; // namespace micasa
