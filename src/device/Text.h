#pragma once

#include "../Device.h"
#include "../Logger.h"

#include "../Scheduler.h"

namespace micasa {

	class Text final : public Device, public Logger::Receiver {

	public:
		enum class SubType: unsigned short {
			GENERIC = 1,
			WIND_DIRECTION,
			NOTIFICATION
		}; // enum class SubType
		static const std::map<SubType, std::string> SubTypeText;
		ENUM_UTIL_W_TEXT( SubType, SubTypeText );

		typedef std::string t_value;
		static const Device::Type type;
		
		Text( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		Device::Type getType() const throw() override { return Text::type; };
		
		void updateValue( const Device::UpdateSource& source_, const t_value& value_ );
		t_value getValue() const throw() { return this->m_value; };
		t_value getPreviousValue() const throw() { return this->m_previousValue; };
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		void putSettingsJson( const nlohmann::json& settings_ ) override;
		nlohmann::json getData( unsigned int range_, const std::string& interval_ ) const;
		void log( const Logger::LogLevel& logLevel_, const std::string& message_ );

	private:
		t_value m_value;
		t_value m_previousValue;
		Scheduler m_scheduler;
		struct {
			t_value value;
			Device::UpdateSource source;
			std::chrono::system_clock::time_point last;
			std::weak_ptr<Scheduler::Task<> > task;
		} m_rateLimiter;

		void _init() override;
		void _processValue( const Device::UpdateSource& source_, const t_value& value_ );
		void _purgeHistory() const;

	}; // class Text

}; // namespace micasa
