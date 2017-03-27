#pragma once

#include <string>

#include "Hardware.h"
#include "Utils.h"

#define DEVICE_TYPE_COUNTER 0x1
#define DEVICE_TYPE_LEVEL   0x2
#define DEVICE_TYPE_SWITCH  0x3
#define DEVICE_TYPE_TEXT    0x4

namespace micasa {

	// ==========
	// DeviceBase
	// ==========

	class DeviceBase {
	
	public:
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

		DeviceBase( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_, bool enabled_ ) :
			m_hardware( hardware_ ),
			m_id( id_ ),
			m_reference( reference_ ),
			m_label( label_ ),
			m_enabled( enabled_ )
		{
		};
		virtual ~DeviceBase() = 0;

		virtual nlohmann::json getJson( bool full_ = false ) const;

	protected:
		std::shared_ptr<Hardware> m_hardware;
		const unsigned int m_id;
		const std::string m_reference;
		std::string m_label;
		bool m_enabled;
		std::shared_ptr<Settings<Device> > m_settings;

	}; // class DeviceBase

	// =======
	// Counter
	// =======

	class Counter2 : public DeviceBase {
	
	public:
		static const unsigned char type = DEVICE_TYPE_COUNTER;
		typedef double t_value;

		using DeviceBase::DeviceBase;
		virtual ~Counter2() = 0;

	}; // class Counter

	// =====
	// Level
	// =====

	class Level2 : public DeviceBase {

	public:
		static const unsigned char type = DEVICE_TYPE_LEVEL;
		typedef double t_value;

		using DeviceBase::DeviceBase;
		virtual ~Level2() = 0;

	}; // class Level

	// ======
	// Switch
	// ======

	class Switch2 : public DeviceBase {

	public:
		static const unsigned char type = DEVICE_TYPE_SWITCH;
		typedef std::string t_value;

		using DeviceBase::DeviceBase;
		virtual ~Switch2() = 0;

	}; // class Switch

	// ====
	// Text
	// ====

	class Text2 : public DeviceBase {

	public:
		static const unsigned char type = DEVICE_TYPE_TEXT;
		typedef std::string t_value;

		using DeviceBase::DeviceBase;
		virtual ~Text2() = 0;

	}; // class Text

	// ======
	// Device
	// ======

	template<class D> class Device2 : public D {

	public:
		using D::D; // use constructor of base
		Device2( const Device2& ) = delete; // do not copy
		Device2& operator=( const Device2& ) = delete; // do not copy-assign

		typename D::t_value getValue() const { return this->m_value; };
		unsigned char getType() const { return D::type; };
		void setValue( const DeviceBase::UpdateSource& source_, const typename D::t_value& value_ );
		nlohmann::json getJson( bool full_ = false ) const override;

	private:
		typename D::t_value m_value;
		typename D::t_value m_previousValue;

	}; // class Device

}; // namespace micasa
