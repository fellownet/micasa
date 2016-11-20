#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <vector>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <vector>
#include <map>
#include <mutex>

#include "Logger.h"

namespace micasa {

	class Database final : public LoggerInstance {

	public:
		Database( std::string filename_ );
		~Database();
		std::string toString() const;

		std::vector<std::map<std::string, std::string> > getQuery( std::string query_, ... ) const;
		std::map<std::string, std::string> getQueryRow( std::string query_, ... ) const ;
		std::map<std::string, std::string> getQueryMap( std::string query_, ... ) const ;
		std::string getQueryValue( std::string query_, ... ) const;
		long putQuery( std::string query_, ... ) const;

	private:
		void _init() const;
		void _wrapQuery( std::string query_, va_list arguments_, std::function<void(sqlite3_stmt*)> process_ ) const;

		const std::string m_filename;
		sqlite3 *m_connection;
		mutable std::mutex m_queryMutex;

	}; // class Database

}; // namespace micasa
