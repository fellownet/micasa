#pragma once

#include <string>
#include <vector>
#include <map>

#include <sqlite3.h>

#include "json.hpp"

namespace micasa {

	class Database final {

		friend class System;

	public:
		class NoResultsException: public std::runtime_error {
		public:
			using runtime_error::runtime_error;
		}; // class NoResultsException

		class InvalidResultException: public std::runtime_error {
		public:
			using runtime_error::runtime_error;
		}; // class InvalidResultException

		Database();
		~Database();

		Database( const Database& ) = delete; // do not copy
		Database& operator=( const Database& ) = delete; // do not copy-assign
		Database( const Database&& ) = delete; // do not move
		Database& operator=( Database&& ) = delete; // do not move-assign

		friend std::ostream& operator<<( std::ostream& out_, const Database* ) { out_ << "Database"; return out_; }

		std::vector<std::map<std::string, std::string>> getQuery( const std::string query_, ... ) const;
		template<typename T> T getQuery( const std::string query_, ... ) const;
		std::map<std::string, std::string> getQueryRow( const std::string query_, ... ) const;
		template<typename T> T getQueryRow( const std::string query_, ... ) const;
		template<typename T> std::vector<T> getQueryColumn( const std::string query_, ... ) const;
		std::map<std::string, std::string> getQueryMap( const std::string query_, ... ) const ;
		template<typename T> T getQueryValue( const std::string query_, ... ) const;
		long putQuery( const std::string query_, ... ) const;
		int getLastErrorCode() const;

	private:
		sqlite3 *m_connection;
		mutable unsigned long long m_queries;

		void _init() const;
		void _wrapQuery( const std::string& query_, va_list arguments_, const std::function<void(sqlite3_stmt*)>&& process_ ) const;

	}; // class Database

}; // namespace micasa
