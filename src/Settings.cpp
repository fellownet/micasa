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
	};
	
	void Settings::commit( const Hardware& hardware_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `hardware_settings` (`key`, `value`, `hardware_id`) "
				"VALUES (%Q, %Q, %q)"
				, settingsIt->first.c_str(), settingsIt->second.c_str(), hardware_.getId().c_str()
			);
		}
	};
	
	void Settings::commit( const Device& device_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			g_database->putQuery(
				"REPLACE INTO `device_settings` (`key`, `value`, `device_id`) "
				"VALUES (%Q, %Q, %q)"
				, settingsIt->first.c_str(), settingsIt->second.c_str(), device_.getId().c_str()
			);
		}
	};

	template<typename T> Settings* Settings::put( const std::string& key_, const T& value_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		std::stringstream ss;
		ss << value_;
		this->m_settings[key_] = ss.str();
		return this;
	};
	template Settings* Settings::put( const std::string& key_, const std::string& value_ );
	template Settings* Settings::put( const std::string& key_, const int& value_ );
	template Settings* Settings::put( const std::string& key_, const unsigned int& value_ );
	template Settings* Settings::put( const std::string& key_, const float& value_ );
	template Settings* Settings::put( const std::string& key_, const double& value_ );
	
	template<typename T> T Settings::get( const std::string& key_, const T& default_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		try {
			T value;
			std::istringstream( this->m_settings.at( key_ ) ) >> value;
			return value;
		} catch( std::out_of_range exception_ ) {
			std::stringstream ss;
			ss << default_;
			this->m_settings[key_] = ss.str();
			return default_;
		}
	};
	template std::string Settings::get( const std::string& key_, const std::string& default_ );
	template int Settings::get( const std::string& key_, const int& default_ );
	template unsigned int Settings::get( const std::string& key_, const unsigned int& default_ );
	template float Settings::get( const std::string& key_, const float& default_ );
	template double Settings::get( const std::string& key_, const double& default_ );
	
	const std::string& Settings::operator[]( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		try {
			return this->m_settings.at( key_ );
		} catch( std::out_of_range exception_ ) {
			return NOT_FOUND;
		}
	};
	
}; // namespace micasa
