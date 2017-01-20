#pragma once

#include <string>
#include <memory>
#include <vector>

#include "Settings.h"
#include "Utils.h"

namespace micasa {

	class User {

	public:
		enum class Rights: unsigned short {
			// v Public
			PUBLIC    = 1,
			// v View all devices
			VIEWER    = 2,
			// v Update all device values + VIEWER rights
			USER      = 3,
			// v Add/remove hardware, devices, scripts, timers and devices + USER rights
			INSTALLER = 4,
			// v All rights
			ADMIN     = 99
		}; // enum class Rights
		ENUM_UTIL( Rights );

		static const constexpr char* settingsName = "user";

		User( const unsigned int id_, const std::string& name_, const unsigned short rights_ );
		~User();

		unsigned int getId() const throw() { return this->m_id; };

	private:
		const unsigned int m_id;
		const std::string m_name;
		const unsigned short m_rights;
		std::shared_ptr<Settings<User> > m_settings;

	}; // class User

}; // namespace micasa
