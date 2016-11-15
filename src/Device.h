#pragma once

#include <string>
#include <memory>

namespace micasa {

	class Hardware;

	class Device {

	public:
		typedef enum {
			COUNTER,
			LEVEL,
			SWITCH,
			TEXT,
		} DeviceType;
		
		Device( std::shared_ptr<Hardware> hardware_ ) : m_hardware( hardware_ ) { };
		virtual ~Device() { };
		
		virtual std::string toString() const =0;

	private:
		std::shared_ptr<Hardware> m_hardware;

	}; // class Device

}; // namespace micasa
