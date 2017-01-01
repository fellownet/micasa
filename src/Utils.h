#pragma once

#include <string>
#include <vector>

namespace micasa {

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, const bool strict_, std::string& result_ );
	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ );
	bool stringStartsWith( const std::string& haystack_, const std::string& search_ );
	std::vector<std::string> stringSplit( const std::string& input_, const char& delim_ );
	void stringSplit( const std::string& input_, const char& delim_, std::vector<std::string>& results_ );
	std::string stringUriDecode(const std::string & sSrc);
	std::string randomString( size_t length_ );
	
}; // namespace micasa
