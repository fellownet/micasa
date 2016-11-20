#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <chrono>
#include <mutex>
#include <vector>

#include "WebServer.h"
#include "Worker.h"
#include "Hardware.h"
#include "Settings.h"

namespace micasa {

	class Controller final : public Worker, public LoggerInstance, public WebServer::ResourceHandler, public std::enable_shared_from_this<Controller> {

	public:
		Controller();
		~Controller();

		void start();
		void stop();
		std::string toString() const;
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ );

	protected:
		std::chrono::milliseconds _work( unsigned long int iteration_ );

	private:
		std::vector<std::shared_ptr<Hardware> > m_hardware;
		std::mutex m_hardwareMutex;

		// TODO remove this
	public:
		std::shared_ptr<Hardware> _declareHardware( Hardware::HardwareType hardwareType_, std::string reference_, std::string name_, std::map<std::string, std::string> settings_ );

	}; // class Controller

}; // namespace micasa
