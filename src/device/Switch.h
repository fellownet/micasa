#pragma once

#include <string>

#include "../Device.h"

#include "../Scheduler.h"

namespace micasa {

	class Switch final : public Device {

	public:
		enum class SubType: unsigned short {
			GENERIC = 1,
			LIGHT,
			DOOR_CONTACT,
			BLINDS,
			MOTION_DETECTOR,
			FAN,
			HEATER,
			BELL,
			ACTION
		}; // enum class SubType
		static const std::map<SubType, std::string> SubTypeText;
		ENUM_UTIL_W_TEXT( SubType, SubTypeText );

		enum class Option: unsigned short {
			ON = 1,
			OFF = 2,
			OPEN = 4,
			CLOSE = 8,
			STOP = 16,
			START = 32,
			IDLE = 64,
			ACTIVATE = 128
		}; // enum class Option
		static const std::map<Switch::Option, std::string> OptionText;
		ENUM_UTIL_W_TEXT( Option, OptionText );

		typedef std::string t_value;
		// typedef t_switchValue t_value;
		static const Device::Type type;
		
		Switch( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		Device::Type getType() const throw() override { return Switch::type; };
		
		void updateValue( const Device::UpdateSource& source_, const Option& value_ );
		void updateValue( const Device::UpdateSource& source_, const t_value& value_ );
		Option getValueOption() const throw() { return this->m_value; };
		Option getPreviousValueOption() const throw() { return this->m_previousValue; };
		static Option getOppositeValueOption( const Option& value_ ) throw();
		t_value getValue() const throw() { return OptionText.at( this->m_value ); };
		t_value getPreviousValue() const throw() { return OptionText.at( this->m_previousValue ); };
		static t_value getOppositeValue( const t_value& value_ ) throw();
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		nlohmann::json getData( unsigned int range_, const std::string& interval_ ) const;

	protected:
		//std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

	private:
		Option m_value = Option::OFF;
		Option m_previousValue = Option::OFF;
		Scheduler m_scheduler;
		struct {
			Option value;
			Device::UpdateSource source;
			std::chrono::system_clock::time_point last;
			std::weak_ptr<Scheduler::Task<> > task;
		} m_rateLimiter;

		void _processValue( const Device::UpdateSource& source_, const Option& value_ );
		void _purgeHistory() const;

	}; // class Switch

}; // namespace micasa
