#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include <memory>

#include "Logger.h"
#include "WebServer.h"
#include "Device.h"

namespace micasa {

	class Hardware : public WebServerResource, public LoggerInstance, public std::enable_shared_from_this<Hardware> {

	public:
		typedef enum {
			INTERNAL = 1,
			HARMONY_HUB = 11,
			OPEN_ZWAVE,
			OPEN_ZWAVE_NODE,
			P1_METER,
			PIFACE,
			PIFACE_BOARD,
			RFXCOM,
			SOLAREDGE,
			WEATHER_UNDERGROUND,
			DOMOTICZ,
		} HardwareType;
		
		Hardware( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ );
		virtual ~Hardware();

		virtual std::string toString() const =0;
		
		virtual void start();
		virtual void stop();
		virtual bool handleRequest( std::string resource_, WebServerResource::Method method_, std::map<std::string, std::string> &data_ ) { return true; /* not implemented yet */ };
		
		std::string getId() { return this->m_id; };
		std::string getUnit() { return this->m_unit; };
		std::shared_ptr<Device> declareDevice( Device::DeviceType deviceType_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ );

		static std::shared_ptr<Hardware> factory( HardwareType hardwareType, std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ );

	protected:
		const std::string m_id;
		const std::string m_unit;
		const std::string m_name;
		const std::map<std::string, std::string> m_settings;

	private:
		std::vector<std::shared_ptr<Device> > m_devices;
		std::mutex m_devicesMutex;
		
	}; // class Hardware

}; // namespace micasa
