#pragma once

#include <string>
#include <memory>
#include <map>

#include "WebServer.h"
#include "Settings.h"

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
		
		virtual void start();
		virtual void stop();

		std::string getId() const { return this->m_id; };
		std::string getReference() const { return this->m_reference; };
		std::string getName() const { return this->m_name; };
		Settings& getSettings() { return this->m_settings; };

		// TODO move to private
		static std::shared_ptr<Device> _factory( std::shared_ptr<Hardware> hardware_, const DeviceType deviceType_, const std::string id_, const std::string reference_, std::string name_ );

		
	protected:
		std::shared_ptr<Hardware> m_hardware;
		const std::string m_id;
		const std::string m_reference;
		std::string m_name;
		Settings m_settings;
		
	private:
		
	}; // class Device

}; // namespace micasa
