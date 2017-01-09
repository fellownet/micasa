#pragma once

#include <string>
#include <memory>
#include <map>
#include <chrono>

#include "WebServer.h"
#include "Settings.h"

#define DEVICE_SETTING_ALLOWED_UPDATE_SOURCES "_allowed_update_sources"
#define DEVICE_SETTING_KEEP_HISTORY_PERIOD    "_keep_history_period"
#define DEVICE_SETTING_UNITS                  "_units"
#define DEVICE_SETTING_FLAGS	                  "_flags"
#define DEVICE_SETTING_ALLOW_UNIT_CHANGE      "_allow_unit_change"

namespace micasa {

	using namespace nlohmann;

	class Hardware;

	class Device : public Worker, public std::enable_shared_from_this<Device> {
		
	public:
		enum Type {
			COUNTER = 1,
			LEVEL,
			SWITCH,
			TEXT,
		}; // enum Type
		
		enum UpdateSource {
			INIT = 1,
			HARDWARE = 2,
			TIMER = 4,
			SCRIPT = 8,
			API = 16,
		}; // enum UpdateSource
		
		virtual ~Device();
		friend std::ostream& operator<<( std::ostream& out_, const Device* device_ );

		// This is the preferred way to create a device of specific type (hence the protected
		// constructor).
		static std::shared_ptr<Device> factory( std::shared_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_ );

		unsigned int getId() const { return this->m_id; };
		std::string getReference() const { return this->m_reference; };
		std::string getLabel() const { return this->m_label; };
		std::string getName() const;
		void setLabel( const std::string& label_ );
		template<class T> bool updateValue( const unsigned int& source_, const typename T::t_value& value_ );
		template<class T> typename T::t_value getValue() const;
		std::shared_ptr<Settings> getSettings() const { return this->m_settings; };
		std::shared_ptr<Hardware> getHardware() const { return this->m_hardware; }
		void setScripts( std::vector<unsigned int>& scriptIds_ );
		std::chrono::time_point<std::chrono::system_clock> getLastUpdate() const { return this->m_lastUpdate; };

		virtual json getJson( bool full_ = false ) const;
		virtual Type getType() const =0;
		
	protected:
		Device( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ );

		std::shared_ptr<Hardware> m_hardware;
		const unsigned int m_id;
		const std::string m_reference;
		std::string m_label;
		std::shared_ptr<Settings> m_settings;
		std::chrono::time_point<std::chrono::system_clock> m_lastUpdate;

	}; // class Device

}; // namespace micasa
