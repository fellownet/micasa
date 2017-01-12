#pragma once

#include <string>
#include <vector>
#include <map>
#include "json.hpp"

namespace micasa {

	using namespace nlohmann;

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, const bool strict_, std::string& result_ );
	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ );
	bool stringStartsWith( const std::string& haystack_, const std::string& search_ );
	std::vector<std::string> stringSplit( const std::string& input_, const char& delim_ );
	void stringSplit( const std::string& input_, const char& delim_, std::vector<std::string>& results_ );
	std::string randomString( size_t length_ );
	std::string extractStringFromJson( const json& json_ );
	const std::map<std::string, std::string> getSerialPorts();
	std::map<std::string, std::string> extractSettingsFromJson( const json& data_ );

	bool generateKeys( std::string& public_, std::string& private_ );
	std::string generateHash( const std::string& input_, const std::string& privateKey_ );
	std::string encrypt( const std::string& input_, const std::string& privateKey_ );
	std::string decrypt( const std::string& input_, const std::string& publicKey_ );
	
}; // namespace micasa
