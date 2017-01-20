#pragma once

#include <string>

#include "../Device.h"

namespace micasa {

	class Switch final : public Device {

	public:
		enum class SubType: unsigned short {
			GENERIC = 1,
			LIGHT,
			DOOR_CONTACT,
			BLINDS,
			MOTION_DETECTOR
		}; // enum class SubType
		static const std::map<SubType, std::string> SubTypeText;
		ENUM_UTIL_W_TEXT( SubType, SubTypeText );

		enum class Option: unsigned short {
			ON = 1,
			OFF = 2,
			OPEN = 4,
			CLOSED = 8,
			STOPPED = 16,
			STARTED = 32,
			IDLE = 64,
			ACTIVATED = 128
		}; // enum class Option
		static const std::map<Switch::Option, std::string> OptionText;
		ENUM_UTIL_W_TEXT( Option, OptionText );

		typedef std::string t_value;
		static const Device::Type type;
		
		Switch( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };

		Device::Type getType() const throw() override { return Switch::type; };
		
		void start() override;
		void stop() override;
		bool updateValue( const Device::UpdateSource& source_, const Option& value_ );
		bool updateValue( const Device::UpdateSource& source_, const t_value& value_ );
		const Option getValueOption() const throw() { return this->m_value; };
		const Option getPreviousValueOption() const throw() { return this->m_previousValue; };
		t_value getValue() const throw() { return OptionText.at( this->m_value ); };
		t_value getPreviousValue() const throw() { return OptionText.at( this->m_previousValue ); };
		json getJson( bool full_ = false ) const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

	private:
		Option m_value = Option::OFF;
		Option m_previousValue = Option::OFF;
		
	}; // class Switch

}; // namespace micasa
