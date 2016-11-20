#pragma once

#include <string>
#include <vector>

namespace micasa {

	class Arguments final {

	public:
		Arguments( int &argc_, char **argv_ );

		const std::string get( const std::string &option_ ) const;
		bool exists( const std::string &option_ ) const;

	private:
		std::vector <std::string> _arguments;

	}; // class Arguments

}; // namespace micasa
