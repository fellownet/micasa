#include "Settings.h"

#include "Database.h"
#include "Hardware.h"
#include "Device.h"
#include "Utils.h"

namespace micasa {
	
	extern std::shared_ptr<Database> g_database;

	const std::string Settings::NOT_FOUND = "";
	
	void Settings::insert( const std::map<std::string, std::string>& settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( settings_.begin(), settings_.end() );
		this->m_dirty = true;
	};
	
	bool Settings::contains( const std::initializer_list<std::string>& settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			if ( this->m_settings.find( *settingsIt ) == this->m_settings.end() ) {
				return false;
			}
		}
		return true;
	};

	bool Settings::contains( const std::string& key_ ) {
		return this->m_settings.find( key_ ) != this->m_settings.end();
	};

	unsigned int Settings::count() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_settings.size();
	};
	
	void Settings::populate() {
		this->m_settings.clear();
		auto results = g_database->getQueryMap(
			"SELECT `key`, `value` "
			"FROM `settings` "
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};
	
	void Settings::populate( const Hardware& hardware_ ) {
		this->m_settings.clear();
		auto results = g_database->getQueryMap(
			"SELECT `key`, `value` "
			"FROM `hardware_settings` "
			"WHERE `hardware_id`=%d"
			, hardware_.getId()
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};
	
	void Settings::populate( const Device& device_ ) {
		this->m_settings.clear();
		auto results = g_database->getQueryMap(
			"SELECT `key`, `value` "
			"FROM `device_settings` "
			"WHERE `device_id`=%d"
			, device_.getId()
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};

	void Settings::commit() {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `settings` (`key`, `value`) "
				"VALUES (%Q, %Q)"
				, settingsIt->first.c_str(), settingsIt->second.c_str()
			);
		}
		this->m_dirty = false;
	};
	
	void Settings::commit( const Hardware& hardware_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `hardware_settings` (`key`, `value`, `hardware_id`) "
				"VALUES (%Q, %Q, %d)"
				, settingsIt->first.c_str(), settingsIt->second.c_str(), hardware_.getId()
			);
		}
		this->m_dirty = false;
	};
	
	void Settings::commit( const Device& device_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `device_settings` (`key`, `value`, `device_id`) "
				"VALUES (%Q, %Q, %d)"
				, settingsIt->first.c_str(), settingsIt->second.c_str(), device_.getId()
			);
		}
		this->m_dirty = false;
	};

	std::string Settings::get( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_settings.at( key_ );
	};

	template<typename T> T Settings::get( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		T value;
		std::istringstream( this->m_settings.at( key_ ) ) >> value;
		return value;
	};
	template int Settings::get( const std::string& key_ ) const;
	template unsigned int Settings::get( const std::string& key_ ) const;
	template double Settings::get( const std::string& key_ ) const;
	
	template<> bool Settings::get( const std::string& key_ ) const {
		return this->get( key_ ) == SETTING_TRUE;
	};

	std::string Settings::get( const std::string& key_, const std::string& default_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		try {
			return this->m_settings.at( key_ );
		} catch( std::out_of_range exception_ ) {
			return default_;
		}
	};

	template<typename T> T Settings::get( const std::string& key_, const T& default_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		try {
			T value;
			std::istringstream( this->m_settings.at( key_ ) ) >> value;
			return value;
		} catch( std::out_of_range exception_ ) {
			return default_;
		}
	};
	template int Settings::get( const std::string& key_, const int& default_ ) const;
	template unsigned int Settings::get( const std::string& key_, const unsigned int& default_ ) const;
	template double Settings::get( const std::string& key_, const double& default_ ) const;
	
	template<> bool Settings::get( const std::string& key_, const bool& default_ ) const {
		try {
			return this->get<bool>( key_ );
		} catch( std::out_of_range exception_ ) {
			return default_;
		}
	};
	
	Settings* Settings::put( const std::string& key_, const std::string& value_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		if (
			this->m_settings.find( key_ ) == this->m_settings.end() // does not exist
			|| value_ != this->m_settings.at( key_ ) // is not the same
		) {
			this->m_settings[key_] = value_;
			this->m_dirty = true;
		}
		return this;
	};

	template<typename T> Settings* Settings::put( const std::string& key_, const T& value_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		std::stringstream ss;
		ss << value_;
		if (
			this->m_settings.find( key_ ) == this->m_settings.end() // does not exist
			|| ss.str() != this->m_settings.at( key_ ) // is not the same
		) {
			this->m_settings[key_] = ss.str();
			this->m_dirty = true;
		}
		return this;
	};
	template Settings* Settings::put( const std::string& key_, const int& value_ );
	template Settings* Settings::put( const std::string& key_, const unsigned int& value_ );
	template Settings* Settings::put( const std::string& key_, const double& value_ );

	template<> Settings* Settings::put<bool>( const std::string& key_, const bool& value_ ) {
		std::string value = value_ ? SETTING_TRUE : SETTING_FALSE;
		return this->put( key_, value );
	};

	const std::map<std::string,std::string> Settings::getAll( const std::string& keys_, const bool& keepNotFounds_ ) const {
		std::vector<std::string> keys;
		stringSplit( keys_, ',', keys );
		return this->getAll( keys );
	};
	
	const std::map<std::string,std::string> Settings::getAll( const std::vector<std::string>& keys_, const bool& keepNotFounds_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		std::map<std::string,std::string> results;
		for ( auto keysIt = keys_.begin(); keysIt != keys_.end(); keysIt++ ) {
			std::string value = NOT_FOUND;
			try {
				value = this->m_settings.at( *keysIt );
			} catch( std::out_of_range exception_ ) {
				/* ignore */
			}
			if (
				value != NOT_FOUND
				|| keepNotFounds_
			) {
				results[*keysIt] = value;
			}
		}
		return results;
	};

}; // namespace micasa
