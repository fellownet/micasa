#pragma once

#include <string>
#include <memory>
#include <map>

#include "WebServer.h"
#include "Settings.h"

#define DEVICE_SETTING_ALLOWED_UPDATE_SOURCES	"_allowed_update_sources"
#define DEVICE_SETTING_KEEP_HISTORY_PERIOD		"_keep_history_period"
#define DEVICE_SETTING_UNITS						"_units"

namespace micasa {

	class Hardware;

	class Device : public Worker, public std::enable_shared_from_this<Device> {
		
		friend class Hardware;
		
	public:
		enum DeviceType {
			COUNTER = 1,
			LEVEL,
			SWITCH,
			TEXT,
		}; // enum DeviceType
		
		enum UpdateSource {
			INIT = 1,
			HARDWARE = 2,
			TIMER = 4,
			SCRIPT = 8,
			API = 16,
		}; // enum UpdateSource

		Device( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ );
		virtual ~Device();
		friend std::ostream& operator<<( std::ostream& out_, const Device* device_ ) { out_ << device_->m_name; return out_; }
		virtual const Device::DeviceType getType() const =0;
		
		virtual void start();
		virtual void stop();

		std::string getId() const { return this->m_id; };
		std::string getReference() const { return this->m_reference; };
		std::string getName() const { return this->m_name; };
		Settings& getSettings() { return this->m_settings; };
		std::shared_ptr<Hardware> getHardware() const { return this->m_hardware; }

	protected:
		std::shared_ptr<Hardware> m_hardware;
		const std::string m_id;
		const std::string m_reference;
		std::string m_name;
		Settings m_settings;
		
	private:
		static std::shared_ptr<Device> _factory( std::shared_ptr<Hardware> hardware_, const DeviceType deviceType_, const std::string id_, const std::string reference_, std::string name_ );
		
	}; // class Device

}; // namespace micasa
