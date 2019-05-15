// https://blogs.gnome.org/jnelson/2015/01/06/sqlite-vacuum-and-auto_vacuum/

#include <memory>

#include "Database.h"
#include "Structs.h"
#include "Logger.h"

namespace micasa {

	using namespace nlohmann;

	Database::Database() {
		int result = sqlite3_open_v2( ( std::string( _DATADIR ) + "/micasa.db" ).c_str(), &this->m_connection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_WAL | SQLITE_OPEN_FULLMUTEX, NULL );
		if ( result == SQLITE_OK ) {
			Logger::log( Logger::LogLevel::VERBOSE, this, "Database opened." );
			this->_init();
		} else {
			Logger::log( Logger::LogLevel::ERROR, this, "Unable to open database." );
		}
		this->m_queries = 0;
	};

	Database::~Database() {
		this->putQuery( "PRAGMA optimize" );
		int result = sqlite3_close( this->m_connection );
		if ( SQLITE_OK == result ) {
			Logger::log( Logger::LogLevel::VERBOSE, this, "Database closed." );
		} else {
			const char *error = sqlite3_errmsg( this->m_connection );
			Logger::logr( Logger::LogLevel::ERROR, this, "Database was not closed properly (%s).", error );
		}
	};

	std::vector<std::map<std::string, std::string>> Database::getQuery( const std::string query_, ... ) const {
		std::vector<std::map<std::string, std::string>> result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			int columns = sqlite3_column_count( statement_ );
			while ( true ) {
				if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
					std::map<std::string, std::string> row;
					for ( int column = 0; column < columns; column++ ) {
						std::string key = std::string( reinterpret_cast<const char*>( sqlite3_column_name( statement_, column ) ) );
						const unsigned char* valueC = sqlite3_column_text( statement_, column );
						if ( valueC != NULL ) {
							std::string value = std::string( reinterpret_cast<const char*>( valueC ) );
							row[key] = value;
						} else {
							row[key] = "";
						}
					}
					result.push_back( row );
				} else {
					break;
				}
			}
		} );
		va_end( arguments );

		return result;
	};

	template<> nlohmann::json Database::getQuery( const std::string query_, ... ) const {
		json result = json::array();

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			int columns = sqlite3_column_count( statement_ );
			while ( true ) {
				if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
					json row = json::object();
					for ( int column = 0; column < columns; column++ ) {
						std::string key = std::string( reinterpret_cast<const char*>( sqlite3_column_name( statement_, column ) ) );
						switch( sqlite3_column_type( statement_, column ) ) {
							case SQLITE_INTEGER:
								row[key] = sqlite3_column_int( statement_, column );
								break;
							case SQLITE_FLOAT:
								row[key] = sqlite3_column_double( statement_, column );
								break;
							case SQLITE_TEXT: {
								const unsigned char* value = sqlite3_column_text( statement_, column );
								row[key] = std::string( reinterpret_cast<const char*>( value ) );
								break;
							}
							case SQLITE_BLOB:
								// not supported (yet?)
								break;
							case SQLITE_NULL:
								break;
						}
					}
					result += row;
				} else {
					break;
				}
			}
		} );
		va_end( arguments );

		return result;
	};

	std::map<std::string, std::string> Database::getQueryRow( const std::string query_, ... ) const {
		std::map<std::string, std::string> result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
				int columns = sqlite3_column_count( statement_ );
				for ( int column = 0; column < columns; column++ ) {
					std::string key = std::string( reinterpret_cast<const char*>( sqlite3_column_name( statement_, column ) ) );
					const unsigned char* valueC = sqlite3_column_text( statement_, column );
					if ( valueC != NULL ) {
						std::string value = std::string( reinterpret_cast<const char*>( valueC ) );
						result[key] = value;
					} else {
						result[key] = "";
					}
				}
			} else {
				throw NoResultsException( "resultset doesn't contain any rows" );
			}
		} );
		va_end( arguments );

		return result;
	};

	template<> nlohmann::json Database::getQueryRow( const std::string query_, ... ) const {
		json result = json::object();

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
				int columns = sqlite3_column_count( statement_ );
				for ( int column = 0; column < columns; column++ ) {
					std::string key = std::string( reinterpret_cast<const char*>( sqlite3_column_name( statement_, column ) ) );
					switch( sqlite3_column_type( statement_, column ) ) {
						case SQLITE_INTEGER:
							result[key] = sqlite3_column_int( statement_, column );
							break;
						case SQLITE_FLOAT:
							result[key] = sqlite3_column_double( statement_, column );
							break;
						case SQLITE_TEXT: {
							const unsigned char* value = sqlite3_column_text( statement_, column );
							result[key] = std::string( reinterpret_cast<const char*>( value ) );
							break;
						}
						case SQLITE_BLOB:
							// not supported (yet?)
							break;
						case SQLITE_NULL:
							break;
					}
				}
			} else {
				throw NoResultsException( "resultset doesn't contain any rows" );
			}
		} );
		va_end( arguments );

		return result;
	};

	template<typename T> std::vector<T> Database::getQueryColumn( const std::string query_, ... ) const {
		std::vector<T> result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			while ( true ) {
				if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
					if ( sqlite3_column_count( statement_ ) != 1 ) {
						throw InvalidResultException( "resultset doesn't contain exactly one column" );
					}
					const unsigned char* valueC = sqlite3_column_text( statement_, 0 );
					if ( valueC != NULL ) {
						T value;
						std::istringstream( reinterpret_cast<const char*>( valueC ) ) >> value;
						result.push_back( value );
					}
				} else {
					break;
				}
			}
		} );
		va_end( arguments );

		return result;
	};

	// The above template is specialized for the types listed below.
	template std::vector<int> Database::getQueryColumn( const std::string query_, ... ) const;
	template std::vector<unsigned int> Database::getQueryColumn( const std::string query_, ... ) const;
	template std::vector<unsigned long> Database::getQueryColumn( const std::string query_, ... ) const;
	template std::vector<double> Database::getQueryColumn( const std::string query_, ... ) const;

	// The string variant of the above template doesn't require string streams and has it's own
	// specialized implementation.
	template<> std::vector<std::string> Database::getQueryColumn( const std::string query_, ... ) const {
		std::vector<std::string> result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			while ( true ) {
				if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
					if ( sqlite3_column_count( statement_ ) != 1 ) {
						throw InvalidResultException( "resultset doesn't contain exactly one column" );
					}
					const unsigned char* valueC = sqlite3_column_text( statement_, 0 );
					if ( valueC != NULL ) {
						result.push_back( std::string( reinterpret_cast<const char*>( valueC ) ) );
					}
				} else {
					break;
				}
			}
		} );
		va_end( arguments );

		return result;
	};

	std::map<std::string, std::string> Database::getQueryMap( const std::string query_, ... ) const {
		std::map<std::string, std::string> result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			int columns = sqlite3_column_count( statement_ );
			if ( 2 != columns ) {
				throw InvalidResultException( "resultset doesn't contain exactly two columns" );
			}
			while ( true ) {
				if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
					std::string key = std::string( reinterpret_cast<const char*>( sqlite3_column_text( statement_, 0 ) ) );
					const unsigned char* valueC = sqlite3_column_text( statement_, 1 );
					if ( valueC != NULL ) {
						std::string value = std::string( reinterpret_cast<const char*>( valueC ) );
						result[key] = value;
					} else {
						result[key] = "";
					}
				} else {
					break;
				}
			}
		} );
		va_end( arguments );

		return result;
	};

	template<typename T> T Database::getQueryValue( const std::string query_, ... ) const {
		std::string result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
				if ( sqlite3_column_count( statement_ ) != 1 ) {
					throw InvalidResultException( "resultset doesn't contain exactly one column" );
				}
				const unsigned char* valueC = sqlite3_column_text( statement_, 0 );
				if ( valueC != NULL ) {
					result = std::string( reinterpret_cast<const char*>( valueC ) );
				}
			} else {
				throw NoResultsException( "resultset doesn't contain any rows" );
			}
		} );
		va_end( arguments );

		T value;
		std::istringstream( result ) >> value;
		return value;
	};

	// The above template is specialized for the types listed below.
	template int Database::getQueryValue( const std::string query_, ... ) const;
	template unsigned int Database::getQueryValue( const std::string query_, ... ) const;
	template unsigned long Database::getQueryValue( const std::string query_, ... ) const;
	template double Database::getQueryValue( const std::string query_, ... ) const;

	// The string variant of the above template doesn't require string streams and has it's own
	// specialized implementation.
	template<> std::string Database::getQueryValue<std::string>( const std::string query_, ... ) const {
		std::string result;

		va_list arguments;
		va_start( arguments, query_ );
		this->_wrapQuery( query_, arguments, [&result]( sqlite3_stmt *statement_ ) {
			if ( SQLITE_ROW == sqlite3_step( statement_ ) ) {
				if ( sqlite3_column_count( statement_ ) != 1 ) {
					throw InvalidResultException( "resultset doesn't contain exactly one column" );
				}
				const unsigned char* valueC = sqlite3_column_text( statement_, 0 );
				if ( valueC != NULL ) {
					result = std::string( reinterpret_cast<const char*>( valueC ) );
				}
			} else {
				throw NoResultsException( "resultset doesn't contain any rows" );
			}
		} );
		va_end( arguments );

		return result;
	};

	long Database::putQuery( const std::string query_, ... ) const {
		va_list arguments;
		va_start( arguments, query_ );
		long insertId = -1;
		this->_wrapQuery( query_, arguments, [this,&insertId]( sqlite3_stmt *statement_ ) {
			if ( SQLITE_DONE != sqlite3_step( statement_ ) ) {
				const char *error = sqlite3_errmsg( this->m_connection );
				Logger::logr( Logger::LogLevel::ERROR, this, "Query rejected (%s).", error );
			} else {
				insertId = sqlite3_last_insert_rowid( this->m_connection );
			}
		} );
		va_end( arguments );
		return insertId;
	};

	int Database::getLastErrorCode() const {
		return sqlite3_errcode( this->m_connection );
	};

	void Database::_init() const {
		// OFF = safe from crashes, not from system failures
		// NORMAL = ok
		this->putQuery( "PRAGMA synchronous=NORMAL" );
		this->putQuery( "PRAGMA foreign_keys=ON" );

		Logger::log( Logger::LogLevel::NORMAL, this, "Optimizing database." );
		this->putQuery( "VACUUM" );

		unsigned int version = this->getQueryValue<unsigned int>( "PRAGMA user_version" );
		if ( version < c_queries.size() ) {
			for ( auto queryIt = c_queries.begin() + version; queryIt != c_queries.end(); queryIt++ ) {
				this->putQuery( *queryIt );
			}
			this->putQuery( "PRAGMA user_version=%d", c_queries.size() );
		}
	};

	void Database::_wrapQuery( const std::string& query_, va_list arguments_, const std::function<void(sqlite3_stmt*)>&& process_ ) const {
		if ( ! this->m_connection ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Database not open." );
			return;
		}

		char* query = sqlite3_vmprintf( query_.c_str(), arguments_ );
		if ( ! query ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Out of memory or invalid printf style query." );
			return;
		}

#ifdef _DEBUG
		Logger::log( Logger::LogLevel::DEBUG, this, std::string( query ) );
#endif // _DEBUG

		sqlite3_stmt *statement;
		if ( SQLITE_OK == sqlite3_prepare_v2( this->m_connection, query, -1, &statement, NULL ) ) {
			try {
				process_( statement );
			} catch( ... ) {
				sqlite3_finalize( statement );
				sqlite3_free( query );
				throw; // re-throw exception
			}
			sqlite3_finalize( statement );
			this->m_queries++;
		} else {
			const char* error = sqlite3_errmsg( this->m_connection );
			Logger::logr( Logger::LogLevel::ERROR, this, "Query rejected (%s).", error );
		}

		sqlite3_free( query );
	};

} // namespace micasa
