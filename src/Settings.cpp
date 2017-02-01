#include <memory>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Settings.h"

#include "Database.h"
#include "Hardware.h"
#include "Device.h"
#include "User.h"

namespace micasa {
	
	using namespace nlohmann;

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

	SettingValue::SettingValue( const char* value_ ) {
		this->assign( value_ );
	};

	template<class T> SettingsHelper<T>::SettingsHelper( const T& target_ ) : m_target( target_ ) {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before settings instances." );
#endif // _DEBUG
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

	template<class T> void SettingsHelper<T>::_populateOnce() const {
		// NOTE Only call this method with held lock on settings mutex.
		if ( ! this->m_populated ) {
			this->m_settings.clear();
			auto results = g_database->getQueryMap(
				"SELECT `key`, `value` "
				"FROM `%s_settings` "
				"WHERE `%s_id`=%d",
				T::settingsName,
				T::settingsName,
				this->m_target.getId()
			);
			this->m_settings.insert( results.begin(), results.end() );
			this->m_populated = true;
		}
	};

	// The void-variant of the class is fully specialized, resulting in a fully instantiated type
	// called SettingsHelper<void>.
	SettingsHelper<void>::SettingsHelper() {
#ifdef _DEBUG
		assert( g_database && "Global Database instance should be created before settings instances." );
#endif // _DEBUG
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

	void SettingsHelper<void>::_populateOnce() const {
		// NOTE Only call this method with held lock on settings mutex.
		if ( ! this->m_populated ) {
			this->m_settings.clear();
			auto results = g_database->getQueryMap(
				"SELECT `key`, `value` "
				"FROM `settings` "
			);
			this->m_settings.insert( results.begin(), results.end() );
			this->m_populated = true;
		}
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
		this->_populateOnce();
		this->m_settings.insert( settings_.begin(), settings_.end() );
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			this->m_dirty.push_back( settingsIt->first );
		}
	};
	
	template<class T> bool Settings<T>::contains( const std::initializer_list<std::string>& settings_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			if ( this->m_settings.find( *settingsIt ) == this->m_settings.end() ) {
				return false;
			}
		}
		return true;
	};

	template<class T> bool Settings<T>::contains( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		return this->m_settings.find( key_ ) != this->m_settings.end();
	};

	template<class T> void Settings<T>::remove( const std::string& key_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		this->m_settings.erase( key_ );
		this->m_dirty.push_back( key_ );
	};

	template<class T> unsigned int Settings<T>::count() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		return this->m_settings.size();
	};

	template<class T> bool Settings<T>::isDirty() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		return this->m_dirty.size() > 0;
	};

	template<class T> std::string Settings<T>::get( const std::string& key_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		return this->m_settings.at( key_ );
	};

	template<class T> std::string Settings<T>::get( const std::string& key_, const std::string& default_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		try {
			return this->m_settings.at( std::string( key_ ) );
		} catch( std::out_of_range exception_ ) {
			return default_;
		}
	};

	template<class T> void Settings<T>::put( const std::string& key_, const SettingValue& value_ ) {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		if (
			this->m_settings.find( key_ ) == this->m_settings.end() // does not exist
			|| value_ != this->m_settings.at( key_ ) // is not the same
		) {
			this->m_settings[key_] = value_;
			this->m_dirty.push_back( key_ );
		}
	};

	template<class T> std::map<std::string, std::string> Settings<T>::getAll() const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		std::map<std::string, std::string> result;
		result.insert( this->m_settings.begin(), this->m_settings.end() );
		return result;
	};

	template<class T> std::map<std::string, std::string> Settings<T>::getAll( const std::string& prefix_ ) const {
		std::lock_guard<std::mutex> lock( this->m_settingsMutex );
		this->_populateOnce();
		std::map<std::string, std::string> result;
		for ( auto settingsIt = this->m_settings.begin(); settingsIt != this->m_settings.end(); settingsIt++ ) {
			if ( settingsIt->first.substr( 0, prefix_.size() ) == prefix_ ) {
				result[settingsIt->first] = settingsIt->second;
			}
		}
		return result;
	};

	template<class T> bool Settings<T>::verifiedPut( const json& defines_, const json& input_, std::string& error_ ) {
		for ( auto definesIt = defines_.begin(); definesIt != defines_.end(); definesIt++ ) {
			auto define = *definesIt;
			auto find = input_.find( define["name"].get<std::string>() );
			if ( find != input_.end() ) {
				const std::string& type = define["type"].get<std::string>();
				if ( type == "double" ) {

					double value;
					if ( (*find).is_number() ) {
						value = (*find).get<double>();
					} else if ( (*find).is_string() ) {
						value = std::stod( (*find).get<std::string>() );
					} else {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (must be number).";
					}
					if (
						( find = define.find( "minimum" ) ) != define.end()
						&& value < (*find).get<double>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too low, minimum = " + std::to_string( (*find).get<double>() ) + ").";
						return false;
					}
					if (
						( find = define.find( "maximum" ) ) != define.end()
						&& value > (*find).get<double>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too high, maximum = " + std::to_string( (*find).get<double>() ) + ").";
						return false;
					}
					this->put( define["name"].get<std::string>(), value );

				} else if ( type == "boolean" ) {
				
					bool value;
					if ( (*find).is_boolean() ) {
						value = (*find).get<bool>();
					} else if ( (*find).is_number() ) {
						value = (*find).get<unsigned int>() > 0;
					} else if ( (*find).is_string() ) {
						value = ( (*find).get<std::string>() == "1" || (*find).get<std::string>() == "true" || (*find).get<std::string>() == "yes" );
					} else {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (must be boolean).";
					}
					this->put( define["name"].get<std::string>(), value );
					
					// If this boolean property was set additional settings might be available which also need to be
					// stored in the settings object.
					if (
						value
						&& ( find = define.find( "settings" ) ) != define.end()
					) {
						this->verifiedPut( *find, input_, error_ );
					}
				
				} else if ( type == "byte" ) {
				
					unsigned char value;
					if ( (*find).is_number() ) {
						value = (*find).get<unsigned char>();
					} else if ( (*find).is_string() ) {
						value = std::stoi( (*find).get<std::string>() );
					} else {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (must be number).";
					}
					if (
						( find = define.find( "minimum" ) ) != define.end()
						&& value < (*find).get<unsigned int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too low, minimum = " + std::to_string( (*find).get<unsigned char>() ) + ").";
						return false;
					}
					if (
						( find = define.find( "maximum" ) ) != define.end()
						&& value > (*find).get<unsigned int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too high, maximum = " + std::to_string( (*find).get<unsigned char>() ) + ").";
						return false;
					}
					this->put( define["name"].get<std::string>(), value );
				
				} else if ( type == "short" ) {
				
					short value;
					if ( (*find).is_number() ) {
						value = (*find).get<short>();
					} else if ( (*find).is_string() ) {
						value = std::stoi( (*find).get<std::string>() );
					} else {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (must be number).";
					}
					if (
						( find = define.find( "minimum" ) ) != define.end()
						&& value < (*find).get<int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too low, minimum = " + std::to_string( (*find).get<short>() ) + ").";
						return false;
					}
					if (
						( find = define.find( "maximum" ) ) != define.end()
						&& value > (*find).get<int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too high, maximum = " + std::to_string( (*find).get<short>() ) + ").";
						return false;
					}
					this->put( define["name"].get<std::string>(), value );
				
				} else if ( type == "int" ) {

					int value;
					if ( (*find).is_number() ) {
						value = (*find).get<int>();
					} else if ( (*find).is_string() ) {
						value = std::stoi( (*find).get<std::string>() );
					} else {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (must be number).";
					}
					if (
						( find = define.find( "minimum" ) ) != define.end()
						&& value < (*find).get<int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too low, minimum = " + std::to_string( (*find).get<int>() ) + ").";
						return false;
					}
					if (
						( find = define.find( "maximum" ) ) != define.end()
						&& value > (*find).get<int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value too high, maximum = " + std::to_string( (*find).get<int>() ) + ").";
						return false;
					}
					this->put( define["name"].get<std::string>(), value );

				} else if ( type == "list" ) {
				
					std::string value = (*find).get<std::string>();
					bool exists = false;
					bool hasSettings = false;
					for ( auto itemsIt = define["options"].begin(); itemsIt != define["options"].end(); itemsIt++ ) {
						if ( (*itemsIt)["value"] == value ) {
							exists = true;
							if ( ( find = (*itemsIt).find( "settings" ) ) != (*itemsIt).end() ) {
								hasSettings = true;
							}
							break;
						}
					}
					if ( ! exists ) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (value not in list).";
						return false;
					}
					this->put( define["name"].get<std::string>(), value );
					
					// If a certain list option is selected that has it's own additional settings then these settings need
					// to be stored aswell.
					if ( hasSettings ) {
						this->verifiedPut( *find, input_, error_ );
					}
				
				} else if ( type == "string" ) {

					std::string value = (*find).get<std::string>();
					if (
						( find = define.find( "maxlength" ) ) != define.end()
						&& value.size() > (*find).get<unsigned int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (too long, maximum length = " + std::to_string( (*find).get<unsigned int>() ) + ").";
						return false;
					}
					if (
						( find = define.find( "minlength" ) ) != define.end()
						&& value.size() < (*find).get<unsigned int>()
					) {
						error_ = "Invalid value for '" + define["label"].get<std::string>() + "' (too short, minimum length = " + std::to_string( (*find).get<unsigned int>() ) + ").";
						return false;
					}
					this->put( define["name"].get<std::string>(), value );

				}
			}
		}
		
		return true;
	};

	template class Settings<void>;
	template class Settings<Hardware>;
	template class Settings<Device>;
	template class Settings<User>;

}; // namespace micasa
