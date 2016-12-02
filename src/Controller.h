#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <iostream>
#include <list>

#include "WebServer.h"
#include "Worker.h"
#include "Hardware.h"
#include "Settings.h"

#include "json.hpp"

namespace micasa {

	class Controller final : public Worker, public std::enable_shared_from_this<Controller> {

	public:
		typedef std::chrono::time_point<std::chrono::system_clock> t_scheduled;
		
		struct Task {
			std::shared_ptr<Device> device;
			Device::UpdateSource source;
			t_scheduled scheduled;
		};
		
		Controller();
		~Controller();
		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }

		void start();
		void stop();
		
		std::shared_ptr<Hardware> getHardware( const std::string reference_ ) const;
		std::shared_ptr<Hardware> declareHardware( const Hardware::HardwareType hardwareType_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ );
		std::shared_ptr<Hardware> declareHardware( const Hardware::HardwareType hardwareType_, const std::shared_ptr<Hardware> parent_, const std::string reference_, const std::string name_, const std::map<std::string, std::string> settings_ );
		template<class D> void newEvent( const D& device_, const Device::UpdateSource& source_ );
		void addTask( const std::shared_ptr<Task> task_ );
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		std::vector<std::shared_ptr<Hardware> > m_hardware;
		mutable std::mutex m_hardwareMutex;
		std::list<std::shared_ptr<Task> > m_taskQueue;
		mutable std::mutex m_taskQueueMutex;
		
		void _processEvent( const nlohmann::json& event_ );
		
	}; // class Controller

}; // namespace micasa
