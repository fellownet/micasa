#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <map>
#include <mutex>
#include <vector>
#include <memory>

#include "Logger.h"
#include "WebServer.h"
#include "Device.h"
#include "Settings.h"

namespace micasa {

	class Hardware : public WebServer::ResourceHandler, public LoggerInstance, public std::enable_shared_from_this<Hardware> {
		
		friend class Controller;

	public:
		enum HardwareType {
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
		};
		
		Hardware( const std::string id_, const std::string reference_, std::string name_ );
		virtual ~Hardware();

		virtual std::string toString() const =0;
		
		virtual void start();
		virtual void stop();
		
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ );
		
		std::string getId() const { return this->m_id; };
		std::string getReference() const { return this->m_reference; };
		Settings& getSettings() { return this->m_settings; };
		
		virtual void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) =0;

	protected:
		const std::string m_id;
		const std::string m_reference;
		std::string m_name;
		Settings m_settings;

		std::shared_ptr<Device> _declareDevice( Device::DeviceType deviceType_, std::string reference_, std::string name_, std::map<std::string, std::string> settings_ );

	private:
		std::vector<std::shared_ptr<Device> > m_devices;
		std::mutex m_devicesMutex;

		static std::shared_ptr<Hardware> _factory( HardwareType hardwareType, std::string id_, std::string reference_, std::string name_ );

	}; // class Hardware

}; // namespace micasa
