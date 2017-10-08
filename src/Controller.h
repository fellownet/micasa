#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <list>

#include "Plugin.h"
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

#define CONTROLLER_SETTING_USERDATA "_userdata"

extern "C" {
	#include "v7.h"

	v7_err micasa_v7_update_device( struct v7*, v7_val_t* );
	v7_err micasa_v7_get_device( struct v7*, v7_val_t* );
	v7_err micasa_v7_include( struct v7*, v7_val_t* );
	v7_err micasa_v7_log( struct v7*, v7_val_t* );
} // extern "C"

namespace micasa {

	class Controller final : public std::enable_shared_from_this<Controller> {

		friend std::ostream& operator<<( std::ostream& out_, const Controller* ) { out_ << "Controller"; return out_; }
		friend v7_err (::micasa_v7_update_device)( struct v7*, v7_val_t* );
		friend v7_err (::micasa_v7_include)( struct v7*, v7_val_t* );

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

		Controller( const Controller& ) = delete; // do not copy
		Controller& operator=( const Controller& ) = delete; // do not copy-assign
		Controller( const Controller&& ) = delete; // do not move
		Controller& operator=( Controller&& ) = delete; // do not move-assign

		void start();
		void stop();

		std::shared_ptr<Plugin> getPlugin( const std::string& reference_ ) const;
		std::shared_ptr<Plugin> getPluginById( const unsigned int& id_ ) const;
		std::vector<std::shared_ptr<Plugin>> getAllPlugins() const;
		std::shared_ptr<Plugin> declarePlugin( const Plugin::Type type_, const std::string reference_, const std::vector<Setting>& settings_, bool enabled_ );
		std::shared_ptr<Plugin> declarePlugin( const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_, const std::vector<Setting>& settings_, bool enabled_ );
		void removePlugin( const std::shared_ptr<Plugin> plugin_ );

		std::shared_ptr<Device> getDevice( const std::string& reference_ ) const;
		std::shared_ptr<Device> getDeviceById( const unsigned int& id_ ) const;
		std::shared_ptr<Device> getDeviceByName( const std::string& name_ ) const;
		std::shared_ptr<Device> getDeviceByLabel( const std::string& label_ ) const;
		std::vector<std::shared_ptr<Device>> getAllDevices() const;
		bool isScheduled( std::shared_ptr<const Device> device_ ) const;
		std::chrono::seconds nextSchedule( std::shared_ptr<const Device> device_ ) const;

		template<class D> void newEvent( std::shared_ptr<D> device_, const Device::UpdateSource& source_ );

#ifdef _WITH_LIBUDEV
		void addSerialPortCallback( const std::string& name_, const t_serialPortCallback& callback_ );
		void removeSerialPortCallback( const std::string& name_ );
#endif // _WITH_LIBUDEV

	private:
		volatile bool m_running;
		std::unordered_map<std::string, std::shared_ptr<Plugin>> m_plugins;
		mutable std::recursive_mutex m_pluginsMutex;
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

		template<class D> void _processTask( std::shared_ptr<D> device_, const typename D::t_value value_, const Device::UpdateSource source_, const TaskOptions options_ );
		void _runScripts( const std::string key_, const nlohmann::json data_, const std::vector<std::map<std::string, std::string>> scripts_ );
		void _runTimers();
		void _runLinks( std::shared_ptr<Device> device_ );
		TaskOptions _parseTaskOptions( const std::string& options_ ) const;

		template<class D> void _js_updateDevice( const std::shared_ptr<D> device_, const typename D::t_value& value_, const std::string& options_ = "" );
		bool _js_include( const std::string& name_, std::string& script_ );

	}; // class Controller

}; // namespace micasa
