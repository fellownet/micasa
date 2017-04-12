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
			// v View all devices
			VIEWER    = 1,
			// v Update all device values + VIEWER rights
			USER      = 2,
			// v Add/remove hardware, devices, scripts, timers and devices + USER rights
			INSTALLER = 3,
			// v All rights
			ADMIN     = 99
		}; // enum class Rights
		ENUM_UTIL( Rights );

		static const char* settingsName;

		User( const unsigned int id_, const std::string name_, const Rights rights_ );
		~User();

		unsigned int getId() const throw() { return this->m_id; };
		Rights getRights() const throw() { return this->m_rights; };
		std::string getName() const throw() { return this->m_name; };
		std::shared_ptr<Settings<User> > getSettings() const throw() { return this->m_settings; };

	private:
		const unsigned int m_id;
		const std::string m_name;
		const Rights m_rights;
		std::shared_ptr<Settings<User> > m_settings;

	}; // class User

}; // namespace micasa
