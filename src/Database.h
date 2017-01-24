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

#include "json.hpp"

namespace micasa {

	class Database final {

	public:
		struct NoResultsException: public std::runtime_error {
		public:
			using runtime_error::runtime_error;
		}; // struct NoResultsException

		struct InvalidResultException: public std::runtime_error {
		public:
			using runtime_error::runtime_error;
		}; // struct InvalidResultException

		Database( std::string filename_ );
		~Database();
		friend std::ostream& operator<<( std::ostream& out_, const Database* ) { out_ << "Database"; return out_; }

		std::vector<std::map<std::string, std::string> > getQuery( const std::string query_, ... ) const;
		template<typename T> T getQuery( const std::string query_, ... ) const;
		std::map<std::string, std::string> getQueryRow( const std::string query_, ... ) const;
		template<typename T> T getQueryRow( const std::string query_, ... ) const;
		template<typename T> std::vector<T> getQueryColumn( const std::string query_, ... ) const;
		std::map<std::string, std::string> getQueryMap( const std::string query_, ... ) const ;
		template<typename T> T getQueryValue( const std::string query_, ... ) const;
		long putQuery( const std::string query_, ... ) const;
		int getLastErrorCode() const;

	private:
		void _init() const;
		void _wrapQuery( const std::string& query_, va_list arguments_, const std::function<void(sqlite3_stmt*)> process_ ) const;

		const std::string m_filename;
		sqlite3 *m_connection;
		mutable std::mutex m_queryMutex;

	}; // class Database

}; // namespace micasa
