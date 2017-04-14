#pragma once

#include "../Device.h"

#include "../Scheduler.h"

namespace micasa {

	class Counter final : public Device {

	public:
		enum class SubType: unsigned short {
			GENERIC = 1,
			ENERGY,
			GAS,
			WATER
		}; // enum class SubType
		static const std::map<SubType, std::string> SubTypeText;
		ENUM_UTIL_W_TEXT( SubType, SubTypeText );

		enum class Unit: unsigned short {
			GENERIC = 1,
			KILOWATTHOUR,
			M3
		}; // enum class Unit
		static const std::map<Unit, std::string> UnitText;
		ENUM_UTIL_W_TEXT( Unit, UnitText );
		
		typedef double t_value;
		static const Device::Type type;

		Counter( std::weak_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );
	
		void updateValue( const Device::UpdateSource& source_, const t_value& value_, bool force_ = false );
		void incrementValue( const Device::UpdateSource& source_, const t_value& value_ = 1.0f );
		t_value getValue() const { return this->m_value; };
		t_value getPreviousValue() const { return this->m_previousValue; };
		nlohmann::json getData( unsigned int range_, const std::string& interval_, const std::string& group_ ) const;

		void start() override;
		void stop() override;
		Device::Type getType() const override { return Counter::type; };
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	private:
		t_value m_value;
		t_value m_previousValue;
		std::chrono::system_clock::time_point m_updated;
		struct {
			t_value value;
			Device::UpdateSource source;
			std::weak_ptr<Scheduler::Task<>> task;
		} m_rateLimiter;

		void _processValue( const Device::UpdateSource& source_, const t_value& value_ );
		void _processTrends() const;
		void _purgeHistory() const;

	}; // class Counter

}; // namespace micasa
