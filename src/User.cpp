#include "User.h"

#include "Logger.h"
#include "Settings.h"

namespace micasa {

	const char* User::settingsName = "user";

	User::User( const unsigned int id_, const std::string name_, Rights rights_ ) : m_id( id_ ), m_name( name_ ), m_rights( rights_ ) {
		this->m_settings = std::make_shared<Settings<User> >( *this );
	};

	User::~User() {
	};

} // namespace micasa
