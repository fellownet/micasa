#pragma once

#include <string>
#include <memory>
#include <map>
#include <chrono>

#include "WebServer.h"
#include "Settings.h"

#define DEVICE_SETTING_ALLOWED_UPDATE_SOURCES	"_allowed_update_sources"
#define DEVICE_SETTING_KEEP_HISTORY_PERIOD		"_keep_history_period"
#define DEVICE_SETTING_UNITS						"_units"
#define DEVICE_SETTING_FLAGS						"_flags"

namespace micasa {

	using namespace nlohmann;

	class Hardware;

	class Device : public Worker, public std::enable_shared_from_this<Device> {
		
		friend class Hardware;
		
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
		
		Device( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ );
		virtual ~Device();
		friend std::ostream& operator<<( std::ostream& out_, const Device* device_ );
		virtual const Type getType() const =0;
		
		const unsigned int getId() const { return this->m_id; };
		const std::string getReference() const { return this->m_reference; };
		const std::string& getLabel() const { return this->m_label; };
		const std::string getName() const;
		void setLabel( const std::string& label_ );
		template<class T> bool updateValue( const unsigned int& source_, const typename T::t_value& value_ );
		template<class T> const typename T::t_value& getValue() const;
		Settings& getSettings() { return this->m_settings; };
		std::shared_ptr<Hardware> getHardware() const { return this->m_hardware; }
		virtual json getJson() const;
		void setScripts( std::vector<unsigned int>& scriptIds_ );
		
	protected:
		std::shared_ptr<Hardware> m_hardware;
		const unsigned int m_id;
		const std::string m_reference;
		std::string m_label;
		Settings m_settings;
		std::chrono::time_point<std::chrono::system_clock> m_lastUpdate;
		
	private:
		static std::shared_ptr<Device> _factory( std::shared_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_ );

	}; // class Device

}; // namespace micasa
