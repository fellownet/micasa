#include "Utils.h"

#include <algorithm>
#include <sstream>
#include <iostream>

namespace micasa {

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, const bool strict_, std::string& result_ ) {
		size_t startPos = haystack_.find( start_ );
		if ( startPos != std::string::npos ) {
			size_t endPos = haystack_.substr( startPos + start_.size() ).find( end_ );
			if ( endPos != std::string::npos ) {
				result_ = haystack_.substr( startPos + start_.size(), endPos );
				return true;
			} else if ( ! strict_ ) {
				result_ = haystack_.substr( startPos + start_.size() );
				return true;
			}
		}
		return false;
	};

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ ) {
		return stringIsolate( haystack_, start_, end_, true, result_ );
	};
	
	bool stringStartsWith( const std::string& haystack_, const std::string& search_ ) {
		return std::equal( search_.begin(), search_.end(), haystack_.begin() );
	};
	
	std::vector<std::string> stringSplit( const std::string& input_, const char& delim_ ) {
		std::vector<std::string> results;
		stringSplit( input_, delim_, results );
		return results;
	};
	
	void stringSplit( const std::string& input_, const char& delim_, std::vector<std::string>& results_ ) {
		std::string token = "";
		std::stringstream stream( input_ );
		while( std::getline( stream, token, delim_ ) ) {
			results_.push_back( token );
		}
	};
	
	std::string randomString( size_t length_ ) {
		auto randchar = []() -> char {
			const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
			const size_t max_index = (sizeof(charset) - 1);
			return charset[ rand() % max_index ];
		};
		std::string str( length_,0 );
		std::generate_n( str.begin(), length_, randchar );
		return str;
	};
	
	const std::vector<std::string> getSerialPorts() {
		std::vector<std::string> results;
		
		
		return results;
	};
	
} // namespace micasa
