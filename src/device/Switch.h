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
			IDLE = 64,
			ACTIVATED = 128
		}; // enum Option

		static const std::map<Switch::Option, std::string> OptionText;
		
		typedef std::string t_value;
		
		Switch( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };
		const Device::Type getType() const { return Device::Type::SWITCH; };
		
		void start() override;
		void stop() override;
		bool updateValue( const unsigned int& source_, const Option& value_ );
		bool updateValue( const unsigned int& source_, const t_value& value_ );
		const unsigned int getValueOption() const { return this->m_value; };
		const t_value& getValue() const { return OptionText.at( this->m_value ); };
		
		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		Option m_value;
		
	}; // class Switch

}; // namespace micasa
