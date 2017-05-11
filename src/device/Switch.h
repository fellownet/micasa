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
			SCENE,
			ACTION = 99 // reserved for hardware actions such as network healing or adding devices
		}; // enum class SubType
		static const std::map<SubType, std::string> SubTypeText;
		ENUM_UTIL_W_TEXT( SubType, SubTypeText );

		enum class Option: unsigned int {
			ON       = (1 << 0),
			OFF      = (1 << 1),
			OPEN     = (1 << 2),
			CLOSE    = (1 << 3),
			STOP     = (1 << 4),
			START    = (1 << 5),
			ENABLED  = (1 << 6),
			DISABLED = (1 << 7),
			IDLE     = (1 << 8),
			ACTIVATE = (1 << 9)
		}; // enum class Option
		static const std::map<Switch::Option, std::string> OptionText;
		ENUM_UTIL_W_TEXT( Option, OptionText );

		typedef std::string t_value;
		static const Device::Type type;
		
		Switch( std::weak_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		void updateValue( const Device::UpdateSource& source_, const Option& value_ );
		void updateValue( const Device::UpdateSource& source_, const t_value& value_ );
		Option getValueOption() const { return this->m_value; };
		static Option getOppositeValueOption( const Option& value_ );
		t_value getValue() const { return OptionText.at( this->m_value ); };
		static t_value getOppositeValue( const t_value& value_ );
		nlohmann::json getData( unsigned int range_, const std::string& interval_ ) const;

		void start() override;
		void stop() override;
		Device::Type getType() const override { return Switch::type; };
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	private:
		Option m_value;
		Device::UpdateSource m_source;
		std::chrono::system_clock::time_point m_updated;
		struct {
			Option value;
			Device::UpdateSource source;
			std::weak_ptr<Scheduler::Task<>> task;
		} m_rateLimiter;

		void _processValue( const Device::UpdateSource& source_, const Option& value_ );
		void _purgeHistory() const;

	}; // class Switch

}; // namespace micasa
