#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <list>

#include "Hardware.h"
#include "Settings.h"
#include "Scheduler.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

#include "json.hpp"

#ifdef _WITH_LIBUDEV
	#include <libudev.h>
#endif // _WITH_LIBUDEV

extern "C" {
	#include "v7.h"
	
	v7_err micasa_v7_update_device( struct v7* js_, v7_val_t* res_ );
	v7_err micasa_v7_get_device( struct v7* js_, v7_val_t* res_ );
	v7_err micasa_v7_include( struct v7* js_, v7_val_t* res_ );
	v7_err micasa_v7_log( struct v7* js_, v7_val_t* res_ );
} // extern "C"

namespace micasa {

	class Controller final : public std::enable_shared_from_this<Controller> {

		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }
		friend v7_err (::micasa_v7_update_device)( struct v7* js_, v7_val_t* res_ );
		friend v7_err (::micasa_v7_get_device)( struct v7* js_, v7_val_t* res_ );
		friend v7_err (::micasa_v7_include)( struct v7* js_, v7_val_t* res_ );
		friend v7_err (::micasa_v7_log)( struct v7* js_, v7_val_t* res_ );

#ifdef _WITH_LIBUDEV
		typedef std::function<void( const std::string& serialPort_, const std::string& action_ )> t_serialPortCallback;
#endif // _WITH_LIBUDEV

	public:
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
		
		std::shared_ptr<Hardware> getHardware( const std::string& reference_ ) const;
		std::shared_ptr<Hardware> getHardwareById( const unsigned int& id_ ) const;
		std::vector<std::shared_ptr<Hardware> > getChildrenOfHardware( const Hardware& hardware_ ) const;
		std::vector<std::shared_ptr<Hardware> > getAllHardware() const;
		std::shared_ptr<Hardware> declareHardware( const Hardware::Type type_, const std::string reference_, const std::vector<Setting>& settings_, bool enabled_ );
		std::shared_ptr<Hardware> declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::vector<Setting>& settings_, bool enabled_ );
		void removeHardware( const std::shared_ptr<Hardware> hardware_ );

		std::shared_ptr<Device> getDevice( const std::string& reference_ ) const;
		std::shared_ptr<Device> getDeviceById( const unsigned int& id_ ) const;
		std::shared_ptr<Device> getDeviceByName( const std::string& name_ ) const;
		std::shared_ptr<Device> getDeviceByLabel( const std::string& label_ ) const;
		std::vector<std::shared_ptr<Device> > getAllDevices() const;
		bool isScheduled( std::shared_ptr<const Device> device_ ) const;

		template<class D> void newEvent( std::shared_ptr<D> device_, const Device::UpdateSource& source_ );

#ifdef _WITH_LIBUDEV
		void addSerialPortCallback( const std::string& name_, const t_serialPortCallback& callback_ );
		void removeSerialPortCallback( const std::string& name_ );
#endif // _WITH_LIBUDEV

	private:
		volatile bool m_running;
		std::unordered_map<std::string, std::shared_ptr<Hardware> > m_hardware;
		mutable std::mutex m_hardwareMutex;
		Scheduler m_scheduler;
		v7* m_v7_js;
		mutable std::mutex m_jsMutex;
		
#ifdef _WITH_LIBUDEV
		std::map<std::string, t_serialPortCallback> m_serialPortCallbacks;
		mutable std::mutex m_serialPortCallbacksMutex;
		udev* m_udev;
		udev_monitor* m_udevMonitor;
		std::thread m_udevWorker;
#endif // _WITH_LIBUDEV
		
		template<class D> void _processTask( std::shared_ptr<D> device_, const typename D::t_value& value_, const Device::UpdateSource& source_, const TaskOptions& options_ );
		void _runScripts( const std::string& key_, const nlohmann::json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ );
		void _runTimers();
		void _runLinks( std::shared_ptr<Device> device_ );
		TaskOptions _parseTaskOptions( const std::string& options_ ) const;

		template<class D> void _js_updateDevice( const std::shared_ptr<D> device_, const typename D::t_value& value_, const std::string& options_ = "" );
		bool _js_include( const std::string& name_, std::string& script_ );
		void _js_log( const std::string& log_ );
		
	}; // class Controller

}; // namespace micasa
