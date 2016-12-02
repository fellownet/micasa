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

extern "C" {
	#include "v7.h"
	
	v7_err micasa_v7_update_device( struct v7* js_, v7_val_t* res_ );
} // extern "C"

namespace micasa {

	class Controller final : public Worker, public std::enable_shared_from_this<Controller> {

		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }
		friend v7_err (::micasa_v7_update_device)( struct v7* js_, v7_val_t* res_ );

	public:
		typedef std::chrono::time_point<std::chrono::system_clock> t_scheduled;
		
		struct Task {
			std::shared_ptr<Device> device;
			Device::UpdateSource source;
			t_scheduled scheduled;
		};
		
		Controller();
		~Controller();

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
		const bool _updateDeviceFromScript( const unsigned int& deviceId_, const std::string& value_ );
		const bool _updateDeviceFromScript( const unsigned int& deviceId_, const double& value_ );
		std::shared_ptr<Device> _getDeviceById( const unsigned int id_ ) const;
		
	}; // class Controller

}; // namespace micasa
