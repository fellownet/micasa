#pragma once

#include <string>
#include <memory>
#include <map>

#include "WebServer.h"

namespace micasa {

	class Hardware;

	class Device : public WebServerResource, public LoggerInstance {

	public:
		typedef enum {
			COUNTER,
			LEVEL,
			SWITCH,
			TEXT,
		} DeviceType;
		
		Device( std::shared_ptr<Hardware> hardware_, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ );
		virtual ~Device();
		
		virtual std::string toString() const =0;

		virtual bool handleRequest( std::string resource_, WebServerResource::Method method_, std::map<std::string, std::string> &data_ ) { return true; /* not implemented yet */ };

		std::string getId() { return this->m_id; };
		std::string getUnit() { return this->m_unit; };

		static std::shared_ptr<Device> factory( std::shared_ptr<Hardware> hardware_, DeviceType deviceType_, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ );
		
	protected:
		const std::string m_id;
		const std::string m_unit;
		const std::string m_name;
		const std::map<std::string, std::string> m_settings;

	private:
		std::shared_ptr<Hardware> m_hardware;

	}; // class Device

}; // namespace micasa
