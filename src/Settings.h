#pragma once

#include <map>
#include <memory>

namespace micasa {

	class Hardware;
	class Device;
	
	class Settings final {
		
	private:
		std::map<std::string, std::string> m_settings;
		
	public:
		Settings() { };
		~Settings() { };
		
		const static std::string NOT_FOUND;
		
		void insert( std::map<std::string, std::string> settings_ );
		bool contains( std::initializer_list<std::string> settings_ );
		unsigned int count();
		void populate();
		void populate( const Hardware& hardware_ );
		void populate( const Device& device_ );
		Settings( const std::shared_ptr<Device> device_ );
		void commit();
		void commit( const Hardware& hardware_ );
		void commit( const Device& device_ );
		
		const std::string& operator[]( const char* key_ ) const;
		const std::string& operator[]( const std::pair<std::string, std::string>& keyWithDefault_ ) const;
		
	}; // class Settings
	
}; // namespace micasa
