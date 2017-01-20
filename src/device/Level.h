#pragma once

#include "../Device.h"

namespace micasa {

	class Level final : public Device {

	public:
		enum class SubType: unsigned short {
			GENERIC = 1,
			TEMPERATURE,
			HUMIDITY,
			POWER,
			ELECTRICITY,
			CURRENT,
			PRESSURE,
			LUMINANCE,
			THERMOSTAT_SETPOINT,
			BATTERY_LEVEL,
			DIMMER
		}; // enum class SubType
		static const std::map<SubType, std::string> SubTypeText;
		ENUM_UTIL_W_TEXT( SubType, SubTypeText );

		enum class Unit: unsigned short {
			GENERIC = 1,
			PERCENT,
			WATT,
			VOLT,
			AMPERES,
			CELCIUS,
			FAHRENHEIT,
			PASCAL,
			LUX
		}; // enum class Unit
		static const std::map<Unit, std::string> UnitText;
		ENUM_UTIL_W_TEXT( Unit, UnitText );

		typedef double t_value;
		static const Device::Type type;
		
		Level( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };

		Device::Type getType() const throw() override { return Level::type; };
		
		void start() override;
		void stop() override;
		bool updateValue( const Device::UpdateSource& source_, const t_value& value_ );
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
