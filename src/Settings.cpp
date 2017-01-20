#include <memory>

#include "Database.h"
#include "Controller.h"
#include "Hardware.h"
#include "Device.h"
#include "User.h"
#include "Utils.h"

#include "Settings.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {
	
	extern std::shared_ptr<Database> g_database;

	SettingValue::SettingValue( const unsigned int& value_ ) {
		this->assign( std::to_string( value_ ) );
	};
	
	SettingValue::SettingValue( const int& value_ ) {
		this->assign( std::to_string( value_ ) );
	};
	
	SettingValue::SettingValue( const double& value_ ) {
		std::stringstream ss;
		ss << std::fixed << std::setprecision( 3 ) << value_;
		this->assign( ss.str() );
	};
	
	SettingValue::SettingValue( const bool& value_ ) {
		std::stringstream ss;
		ss << std::boolalpha << value_;
		this->assign( ss.str() );
	};
	
	SettingValue::SettingValue( const std::string& value_ ) {
		this->assign( value_ );
	};

	template<class T> SettingsHelper<T>::SettingsHelper( const T& target_ ) : m_target( target_ ) {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before settings instances." );
#endif // _DEBUG
		this->m_settings.clear();
		auto results = g_database->getQueryMap(
			"SELECT `key`, `value` "
			"FROM `%s_settings` "
			"WHERE `%s_id`=%d",
			T::settingsName,
			T::settingsName,
			this->m_target.getId()
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};

	template<class T> void SettingsHelper<T>::commit() {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto dirtyIt = this->m_dirty.begin(); dirtyIt != this->m_dirty.end(); dirtyIt++ ) {
			auto setting = this->m_settings.find( *dirtyIt );
			if ( setting != this->m_settings.end() ) {
				g_database->putQuery(
					"REPLACE INTO `%s_settings` (`key`, `value`, `%s_id`) "
					"VALUES (%Q, %Q, %d)",
					T::settingsName,
					T::settingsName,
					setting->first.c_str(),
					setting->second.c_str(),
					this->m_target.getId()
				);
			} else {
				g_database->putQuery(
					"DELETE FROM `%s_settings` "
					"WHERE `key`=%Q "
					"AND `%s_id`=%d",
					T::settingsName,
					setting->first.c_str(),
					T::settingsName,
					this->m_target.getId()
				);
			}
		}
		this->m_dirty.clear();
	};

	// The void-variant of the class is fully specialized, resulting in a fully instantiated type
	// called SettingsHelper<void>.
	SettingsHelper<void>::SettingsHelper() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before settings instances." );
#endif // _DEBUG
		this->m_settings.clear();
		auto results = g_database->getQueryMap(
			"SELECT `key`, `value` "
			"FROM `settings` "
		);
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( results.begin(), results.end() );
	};

	void SettingsHelper<void>::commit() {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto dirtyIt = this->m_dirty.begin(); dirtyIt != this->m_dirty.end(); dirtyIt++ ) {
			auto setting = this->m_settings.find( *dirtyIt );
			if ( setting != this->m_settings.end() ) {
				g_database->putQuery(
					"REPLACE INTO `settings` (`key`, `value`) "
					"VALUES (%Q, %Q)",
					setting->first.c_str(),
					setting->second.c_str()
				);
			} else {
				g_database->putQuery(
					"DELETE FROM `settings` "
					"WHERE `key`=%Q ",
					( *dirtyIt ).c_str()
				);
			}
		}
		this->m_dirty.clear();
	};

	template class SettingsHelper<void>;
	template class SettingsHelper<Hardware>;
	template class SettingsHelper<Device>;
	template class SettingsHelper<User>;

	// The actual Settings implementation follows below. These are the functions that do not require
	// a specialization of any kind.
	template<class T> Settings<T>::~Settings() {
		if ( this->isDirty() ) {
			this->commit();
		}
	}

	template<class T> void Settings<T>::insert( const std::vector<Setting>& settings_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.insert( settings_.begin(), settings_.end() );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			this->m_dirty.push_back( settingsIt->first );
		}
	};
	
	template<class T> bool Settings<T>::contains( const std::initializer_list<std::string>& settings_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			if ( this->m_settings.find( *settingsIt ) == this->m_settings.end() ) {
				return false;
			}
		}
		return true;
	};

	template<class T> bool Settings<T>::contains( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_settings.find( key_ ) != this->m_settings.end();
	};

	template<class T> void Settings<T>::remove( const std::string& key_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->m_settings.erase( key_ );
		this->m_dirty.push_back( key_ );
	};

	template<class T> unsigned int Settings<T>::count() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_settings.size();
	};

	template<class T> bool Settings<T>::isDirty() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_dirty.size() > 0;
	};

	template<class T> std::string Settings<T>::get( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_settings.at( key_ );
	};

	template<class T> std::string Settings<T>::get( const std::string& key_, const std::string& default_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		try {
			return this->m_settings.at( std::string( key_ ) );
		} catch( std::out_of_range exception_ ) {
			return default_;
		}
	};

	template<class T> void Settings<T>::put( const std::string& key_, const SettingValue& value_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		if (
			this->m_settings.find( key_ ) == this->m_settings.end() // does not exist
			|| value_ != this->m_settings.at( key_ ) // is not the same
		) {
			this->m_settings[key_] = value_;
			this->m_dirty.push_back( key_ );
		}
	};

	template class Settings<void>;
	template class Settings<Hardware>;
	template class Settings<Device>;
	template class Settings<User>;

}; // namespace micasa
