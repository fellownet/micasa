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

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

#include "json.hpp"

extern "C" {
	#include "v7.h"
	
	v7_err micasa_v7_update_device( struct v7* js_, v7_val_t* res_ );
	v7_err micasa_v7_get_device( struct v7* js_, v7_val_t* res_ );
} // extern "C"

namespace micasa {

	using namespace nlohmann;

	class Controller final : public Worker, public std::enable_shared_from_this<Controller> {

		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }
		friend v7_err (::micasa_v7_update_device)( struct v7* js_, v7_val_t* res_ );
		friend v7_err (::micasa_v7_get_device)( struct v7* js_, v7_val_t* res_ );

	public:
		typedef std::chrono::time_point<std::chrono::system_clock> t_scheduled;
		
		// TODO make a class out of this with a templated or function overload setValue method?
		// TODO if the value can be part of the base class as union, a lot of code can be generalized in the
		// base device class too.
		struct Task {
			std::shared_ptr<Device> device;
			unsigned int source;
			t_scheduled scheduled;
			
			Text::t_value textValue;
			Switch::t_value switchValue;
			Level::t_value levelValue;
			Counter::t_value counterValue;
			
			template<class D> void setValue( const typename D::t_value& value_ );
		}; // struct Task
		
		struct TaskOptions {
			double forSec;
			double afterSec;
			int repeat;
			double repeatSec;
			bool clear;
			bool recur;
		}; // struct TaskOptions
		
		Controller();
		~Controller();

		void start();
		void stop();
		
		std::shared_ptr<Hardware> getHardware( const std::string reference_ ) const;
		std::shared_ptr<Hardware> declareHardware( const Hardware::Type type_, const std::string reference_, const std::map<std::string, std::string> settings_, const bool& start_ = false );
		std::shared_ptr<Hardware> declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::map<std::string, std::string> settings_, const bool& start_ = false );
		template<class D> void newEvent( const D& device_, const unsigned int& source_ );

	private:
		std::vector<std::shared_ptr<Hardware> > m_hardware;
		mutable std::mutex m_hardwareMutex;
		std::list<std::shared_ptr<Task> > m_taskQueue;
		mutable std::mutex m_taskQueueMutex;
		mutable std::mutex m_scriptsMutex;
		mutable std::mutex m_cronsMutex;
		Settings m_settings;
		
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		void _runScripts( const std::string& key_, const json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ );
		void _runCrons();
		std::shared_ptr<Device> _getDeviceById( const unsigned int& id_ ) const;
		std::shared_ptr<Device> _getDeviceByLabel( const std::string& label_ ) const;
		template<class D> bool _updateDeviceFromScript( const std::shared_ptr<D>& device_, const typename D::t_value& value_, const std::string& options_ = "" );
		void _scheduleTask( const std::shared_ptr<Task> task_ );
		void _clearTaskQueue( const std::shared_ptr<Device>& device_ );
		const TaskOptions _parseTaskOptions( const std::string& options_ ) const;
		
		void _installHardwareResourceHandlers( const std::shared_ptr<Hardware> hardware_ );
		void _updateScriptResourceHandlers() const;
		void _updateCronResourceHandlers() const;
		
	}; // class Controller

}; // namespace micasa
