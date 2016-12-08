#include "Utils.h"

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
	
	void stringSplit( std::string input_, const std::string& delim_, std::vector<std::string>& results_ ) {
		results_.clear();
		size_t cutAt;
		while( ( cutAt = input_.find( delim_ ) ) != std::string::npos ) {
			results_.push_back( input_.substr( 0, cutAt ) );
			input_ = input_.substr( cutAt + delim_.size() );
		}
		if ( ! input_.empty() ) {
			results_.push_back( input_ );
		}
	};
	
} // namespace micasa
