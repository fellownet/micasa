#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <vector>
#include <string>
#include <sqlite3.h>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

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
		void putQuery( std::string query_, ... ) const;

	private:
		void _init() const;
		void _wrapQuery( std::string query_, va_list arguments_, std::function<void(sqlite3_stmt*)> process_ ) const;

		const std::string m_filename;
		sqlite3 *m_connection;
		mutable std::mutex m_queryMutex;

		const std::vector<std::string> c_queries = {
			"CREATE TABLE IF NOT EXISTS `settings` ( "
			"`hardware_id` INTEGER NOT NULL, "
			"`key` VARCHAR(64) NOT NULL, "
			"`value` TEXT NOT NULL, "
			"`last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
			"PRIMARY KEY (`hardware_id`, `key`) "
			"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE CASCADE )",

			"CREATE TABLE IF NOT EXISTS `hardware` ( "
			"`id` INTEGER PRIMARY KEY, " // functions as sqlite3 _rowid_ when named *exactly* INTEGER PRIMARY KEY
			"`unit` VARCHAR(64) NOT NULL, "
			"`type` INTEGER NOT NULL, "
			"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL )",

			"CREATE TABLE IF NOT EXISTS `devices` ( "
			"`id` INTEGER PRIMARY KEY, "
			"`hardware_id` INTEGER NOT NULL, "
			"`unit` VARCHAR(64) NOT NULL, "
			"`type` INTEGER NOT NULL, "
			"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
			"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE CASCADE )",
		};

	}; // class Database

}; // namespace micasa
