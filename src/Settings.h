#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

namespace micasa {

	class Hardware;
	class Device;
	
	class Settings final {
		
	public:
		Settings() { };
		~Settings() { };
		
		const static std::string NOT_FOUND;
		
		void insert( const std::map<std::string, std::string> settings_ );
		bool contains( const std::initializer_list<std::string> settings_ );
		unsigned int count() const;
		void populate();
		void populate( const Hardware& hardware_ );
		void populate( const Device& device_ );
		void commit() const;
		void commit( const Hardware& hardware_ ) const;
		void commit( const Device& device_ ) const;
		bool isDirty() const { return this->m_dirty; }
		
		std::string get( const std::string& key_, const std::string& default_ ) const;
		template<typename T> T get( const std::string& key_, const T& default_ ) const;
		const std::string& operator[]( const std::string& key_ ) const;
		template<typename T> Settings* put( const std::string& key_, const T& value_ );
		const std::map<std::string,std::string> getAll( const std::string& keys_, const bool& keepNotFounds_ = false ) const;
		const std::map<std::string,std::string> getAll( const std::vector<std::string>& keys_, const bool& keepNotFounds_ = false ) const;
		
	private:
		mutable bool m_dirty = false;
		std::map<std::string, std::string> m_settings;
		mutable std::mutex m_settingsMutex;
		
	}; // class Settings
	
}; // namespace micasa
