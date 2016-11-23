#include "Settings.h"

#include "Database.h"
#include "Hardware.h"
#include "Device.h"

namespace micasa {
	
	extern std::shared_ptr<Database> g_database;

	const std::string Settings::NOT_FOUND = "";
	
	// TODO lock settings
	
	void Settings::insert( const std::map<std::string, std::string> settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( settings_.begin(), settings_.end() );
	}
	
	bool Settings::contains( const std::initializer_list<std::string> settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			if ( this->m_settings.find( *settingsIt ) == this->m_settings.end() ) {
				return false;
			}
		}
		return true;
	}

	unsigned int Settings::count() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_settings.size();
	}
	
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
			"WHERE `hardware_id`=%q"
			, hardware_.getId().c_str()
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};
	
	void Settings::populate( const Device& device_ ) {
		this->m_settings.clear();
		auto results = g_database->getQueryMap(
			"SELECT `key`, `value` "
			"FROM `device_settings` "
			"WHERE `device_id`=%q"
			, device_.getId().c_str()
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};

	void Settings::commit() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `settings` (`key`, `value`) "
				"VALUES (%Q, %Q)"
				, settingsIt->first.c_str(), settingsIt->second.c_str()
			);
		}
	}

	void Settings::commit( const Hardware& hardware_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `hardware_settings` (`key`, `value`, `hardware_id`) "
				"VALUES (%Q, %Q, %q)"
				, settingsIt->first.c_str(), settingsIt->second.c_str(), hardware_.getId().c_str()
			);
		}
	}
	
	void Settings::commit( const Device& device_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `device_settings` (`key`, `value`, `device_id`) "
				"VALUES (%Q, %Q, %q)"
				, settingsIt->first.c_str(), settingsIt->second.c_str(), device_.getId().c_str()
			);
		}
	}
	
	// NOTE: a const char* is used here because the notation for the [] operator with default is ambigious and
	// matches std::string too.
	const std::string& Settings::operator[]( const char* key_ ) const {
		try {
			std::lock_guard<std::mutex> lock( this->m_settingsMutex );
			return this->m_settings.at( key_ );
		} catch( std::out_of_range exception_ ) {
			return Settings::NOT_FOUND;
		}
	}
	 
	const std::string& Settings::operator[]( const std::pair<std::string, std::string>& keyWithDefault_ ) const {
		try {
			std::lock_guard<std::mutex> lock( this->m_settingsMutex );
			return this->m_settings.at( keyWithDefault_.first );
		} catch( std::out_of_range exception_ ) {
			return keyWithDefault_.second;
		}
	}
	
}; // namespace micasa
