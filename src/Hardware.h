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

namespace micasa {

	class Hardware : public WebServerResource, public std::enable_shared_from_this<Hardware> {

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
		
		Hardware( std::string id_, std::map<std::string, std::string> settings_ );
		virtual ~Hardware();

		static std::shared_ptr<Hardware> get( std::string id_, HardwareType hardwareType, std::map<std::string, std::string> settings_ );

		virtual bool start();
		virtual bool stop();
		virtual bool handleRequest( std::string resource_, WebServerResource::Method method_, std::map<std::string, std::string> &data_ ) { return true; /* not implemented yet */ };

		//std::shared_ptr<Device> declareDevice();
		//std::shared_ptr<Device> getDevice();
		
	protected:
		const std::string m_id;
		const std::map<std::string, std::string> m_settings;
		
	}; // class Hardware

}; // namespace micasa
