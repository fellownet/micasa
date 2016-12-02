#pragma once

#include "../Device.h"

namespace micasa {

	class Counter final : public Device {

	public:
		enum Unit {
			
			WATTHOUR = 1,
		}; // enum Unit

		Counter( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ ) : Device( hardware_, id_, reference_, name_ ) { };
		const Device::DeviceType getType() const { return Device::DeviceType::COUNTER; };
		
		void start() override;
		void stop() override;
		bool updateValue( const Device::UpdateSource source_, const int value_ );
		const int getValue() const { return this->m_value; };

		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		int m_value;

	}; // class Counter

}; // namespace micasa
