#pragma once

#include <string>
#include <memory>
#include <map>
#include <chrono>

#include "Worker.h"
#include "Settings.h"
#include "Settings.h"
#include "Utils.h"

#define DEVICE_SETTING_ALLOWED_UPDATE_SOURCES "_allowed_update_sources"
#define DEVICE_SETTING_KEEP_HISTORY_PERIOD    "_keep_history_period"
#define DEVICE_SETTING_DEFAULT_UNIT           "_default_unit"
#define DEVICE_SETTING_ALLOW_UNIT_CHANGE      "_allow_unit_change"
#define DEVICE_SETTING_DEFAULT_SUBTYPE        "_default_subtype"
#define DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE   "_allow_subtype_change"
#define DEVICE_SETTING_ADDED_MANUALLY         "_added_manually"
#define DEVICE_SETTING_MINIMUM_USER_RIGHTS    "_minimum_user_rights"
#define DEVICE_SETTING_BATTERY_LEVEL          "_battery_level"
#define DEVICE_SETTING_SIGNAL_STRENGTH        "_signal_strength"

namespace micasa {

	class Hardware;

	class Device : public std::enable_shared_from_this<Device> {
		
	public:
		enum class Type: unsigned short {
			COUNTER = 1,
			LEVEL,
			SWITCH,
			TEXT,
		}; // enum Type
		static const std::map<Type, std::string> TypeText;
		ENUM_UTIL_W_TEXT( Type, TypeText );
		
		enum class UpdateSource: unsigned short {
			INIT = 1,
			HARDWARE = 2,
			TIMER = 4,
			SCRIPT = 8,
			API = 16,
			LINK = 32,

			USER = TIMER | SCRIPT | API | LINK,
			EVENT = TIMER | SCRIPT | LINK,
			CONTROLLER = INIT | HARDWARE,
			ANY = USER | CONTROLLER,

			INTERNAL = 64 // should always be filtered out by hardware
		}; // enum UpdateSource
		ENUM_UTIL( UpdateSource );

		static const constexpr char* settingsName = "device";

		// Device instances should not be copied nor copy assigned.
		Device( const Device& ) = delete;
		Device& operator=( const Device& ) = delete;
		
		virtual ~Device();
		friend std::ostream& operator<<( std::ostream& out_, const Device* device_ );

		// This is the preferred way to create a device of specific type (hence the protected constructor).
		static std::shared_ptr<Device> factory( std::shared_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		unsigned int getId() const throw() { return this->m_id; };
		std::string getReference() const throw() { return this->m_reference; };
		std::string getLabel() const throw() { return this->m_label; };
		std::string getName() const;
		void setLabel( const std::string& label_ );
		template<class T> void updateValue( const Device::UpdateSource& source_, const typename T::t_value& value_ );
		template<class T> typename T::t_value getValue() const;
		std::shared_ptr<Settings<Device> > getSettings() const throw() { return this->m_settings; };
		std::shared_ptr<Hardware> getHardware() const throw() { return this->m_hardware; }
		void setScripts( std::vector<unsigned int>& scriptIds_ );
		bool isEnabled() const throw() { return this->m_enabled; };
		void setEnabled( bool enabled_ = true );
		void touch();

		virtual nlohmann::json getJson( bool full_ = false ) const;
		virtual nlohmann::json getSettingsJson() const;
		virtual void putSettingsJson( nlohmann::json& settings_ );
		virtual Type getType() const =0;
		
	protected:
		Device( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		std::shared_ptr<Hardware> m_hardware;
		const unsigned int m_id;
		const std::string m_reference;
		bool m_enabled;
		std::string m_label;
		std::shared_ptr<Settings<Device> > m_settings;

	}; // class Device

}; // namespace micasa
