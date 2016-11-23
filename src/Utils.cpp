#include "Utils.h"

namespace micasa {

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ ) {
		size_t startPos = haystack_.find( start_ );
		if ( startPos != std::string::npos ) {
			size_t endPos = haystack_.substr( startPos + start_.size() ).find( end_ );
			if ( endPos != std::string::npos ) {
				result_ = haystack_.substr( startPos + start_.size(), endPos );
				return true;
			}
		}
		return false;
	}

} // namespace micasa
