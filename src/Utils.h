#pragma once

#include <string>
#include <vector>

namespace micasa {

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, const bool strict_, std::string& result_ );
	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ );
	bool stringStartsWith( const std::string& haystack_, const std::string& search_ );
	void stringSplit( std::string input_, const std::string& delim_, std::vector<std::string>& results_ );
	
}; // namespace micasa
