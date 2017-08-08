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

		Text( std::weak_ptr<Plugin> plugin_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		void updateValue( Device::UpdateSource source_, t_value value_ );
		t_value getValue() const { return this->m_value; };
		nlohmann::json getData( unsigned int range_, const std::string& interval_ ) const;

		void start() override;
		void stop() override;
		Device::Type getType() const override { return Text::type; };
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		void putSettingsJson( const nlohmann::json& settings_ ) override;
		void log( const Logger::LogLevel& logLevel_, const std::string& message_ ) override;

	private:
		t_value m_value;
		Device::UpdateSource m_source;
		std::chrono::system_clock::time_point m_updated;
		struct {
			t_value value;
			Device::UpdateSource source;
			std::weak_ptr<Scheduler::Task<>> task;
		} m_rateLimiter;
		struct {
			std::string last;
			unsigned int repeated;
			std::weak_ptr<Scheduler::Task<>> task;
		} m_logger;

		void _processValue( const Device::UpdateSource& source_, const t_value& value_ );
		void _purgeHistory() const;

	}; // class Text

}; // namespace micasa
