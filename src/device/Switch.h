#pragma once

#include <string>

#include "../Device.h"

namespace micasa {

	class Switch final : public Device {

	public:
		enum Option {
			ON = 1,
			OFF = 2,
			OPEN = 4,
			CLOSED = 8,
			STOPPED = 16,
			STARTED = 32,
		}; // enum Option

		static const std::map<Switch::Option, std::string> OptionText;
		
		Switch( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string name_ ) : Device( hardware_, id_, reference_, name_ ) { };
		const Device::DeviceType getType() const { return Device::DeviceType::SWITCH; };
		
		void start() override;
		void stop() override;
		bool updateValue( const Device::UpdateSource source_, const Option value_ );
		bool updateValue( const Device::UpdateSource source_, const std::string& value_ );
		const unsigned int getValueOption() const { return this->m_value; };
		const std::string& getValue() const { return OptionText.at( this->m_value ); };
		
		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		Option m_value;
		
	}; // class Switch

}; // namespace micasa
