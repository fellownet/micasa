#include "User.h"

#include "Database.h"
#include "Controller.h"
#include "Database.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	User::User( const unsigned int id_, const std::string name_, Rights rights_ ) : m_id( id_ ), m_name( name_ ), m_rights( rights_ ) {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before Hardware instances." );
		assert( g_logger && "Global Logger instance should be created before Hardware instances." );
#endif // _DEBUG
		this->m_settings = std::make_shared<Settings<User> >( *this );
	};

	User::~User() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be destroyed after Hardware instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Hardware instances." );
#endif // _DEBUG
	};

} // namespace micasa
