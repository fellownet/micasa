#pragma once

#include <map>
#include <memory>
#include <mutex>

namespace micasa {

	class Hardware;
	class Device;
	
	class Settings final {
		
	private:
		std::map<std::string, std::string> m_settings;
		mutable std::mutex m_settingsMutex;
		
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
		
		const std::string& operator[]( const char* key_ ) const;
		const std::string& operator[]( const std::pair<std::string, std::string>& keyWithDefault_ ) const;
		
	}; // class Settings
	
}; // namespace micasa
