#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <iostream>

#include "WebServer.h"
#include "Worker.h"
#include "Hardware.h"
#include "Settings.h"

namespace micasa {

	class Controller final : public Worker, public WebServer::ResourceHandler, public std::enable_shared_from_this<Controller> {

	public:
		Controller();
		~Controller();
		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }

		void start();
		void stop();
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ );
		std::shared_ptr<Hardware> declareHardware( const Hardware::HardwareType hardwareType_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ );
		std::shared_ptr<Hardware> declareHardware( const Hardware::HardwareType hardwareType_, const std::shared_ptr<Hardware> parent_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ );
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		std::vector<std::shared_ptr<Hardware> > m_hardware;
		mutable std::mutex m_hardwareMutex;

	}; // class Controller

}; // namespace micasa
