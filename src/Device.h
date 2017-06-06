#pragma once

#include <string>
#include <ostream>
#include <mutex>
#include <memory>
#include <map>
#include <chrono>

#include "Settings.h"
#include "Utils.h"
#include "Scheduler.h"

#define DEVICE_SETTING_ALLOWED_UPDATE_SOURCES "_allowed_update_sources"
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
			HARDWARE = (1 << 0),
			TIMER    = (1 << 1),
			SCRIPT   = (1 << 2),
			API      = (1 << 3),
			LINK     = (1 << 4),
			SYSTEM   = (1 << 5),

			USER     = TIMER | SCRIPT | API | LINK,
			EVENT    = TIMER | SCRIPT | LINK,
			ANY      = HARDWARE | TIMER | SCRIPT | API | LINK | SYSTEM,

			INTERNAL = (1 << 6) // should always be filtered out by hardware
		}; // enum UpdateSource
		ENUM_UTIL( UpdateSource );

		static const char* settingsName;

		Device( const Device& ) = delete; // Do not copy!
		Device& operator=( const Device& ) = delete; // Do not copy-assign!
		Device( const Device&& ) = delete; // do not move
		Device& operator=( Device&& ) = delete; // do not move-assign

		virtual ~Device();
		friend std::ostream& operator<<( std::ostream& out_, const Device* device_ );

		// This is the preferred way to create a device of specific type (hence the protected constructor).
		static std::shared_ptr<Device> factory( std::weak_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

		unsigned int getId() const { return this->m_id; };
		std::string getReference() const { return this->m_reference; };
		std::string getLabel() const { return this->m_label; };
		std::string getName() const;
		void setLabel( const std::string& label_ );
		template<class T> void updateValue( const Device::UpdateSource& source_, const typename T::t_value& value_ );
		template<class T> typename T::t_value getValue() const;
		std::shared_ptr<Settings<Device>> getSettings() const { return this->m_settings; };
		std::shared_ptr<Hardware> getHardware() const;
		void setScripts( std::vector<unsigned int>& scriptIds_ );
		bool isEnabled() const { return this->m_enabled; };
		void setEnabled( bool enabled_ = true );

		virtual void start() = 0;
		virtual void stop() = 0;
		virtual nlohmann::json getJson( bool full_ = false ) const;
		virtual nlohmann::json getSettingsJson() const;
		virtual void putSettingsJson( const nlohmann::json& settings_ );
		virtual Type getType() const =0;
	
	protected:
		std::weak_ptr<Hardware> m_hardware;
		const unsigned int m_id;
		const std::string m_reference;
		bool m_enabled;
		std::string m_label;
		Scheduler m_scheduler;
		std::shared_ptr<Settings<Device>> m_settings;

		Device( std::weak_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ );

	}; // class Device

}; // namespace micasa
