#include <algorithm>

#include "Arguments.h"

namespace micasa {

	Arguments::Arguments( int &argc_, char **argv_ ) {
		for ( int i = 1; i < argc_; ++i ) {
			this->_arguments.push_back( std::string( argv_[i] ) );
		}
	};

	const std::string Arguments::get( const std::string &option_ ) const {
		std::vector<std::string>::const_iterator itr;
		itr =  std::find( this->_arguments.begin(), this->_arguments.end(), option_ );
		if (
			itr != this->_arguments.end()
			&& ++itr != this->_arguments.end()
		) {
			return *itr;
		}
		return "";
	};

	bool Arguments::exists( const std::string &option_ ) const {
		return std::find( this->_arguments.begin(), this->_arguments.end(), option_ ) != this->_arguments.end();
	};

}; // namespace micasa
