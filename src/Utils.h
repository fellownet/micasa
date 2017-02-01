#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>

#include "json.hpp"

namespace micasa {

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, const bool strict_, std::string& result_ );
	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ );
	bool stringStartsWith( const std::string& haystack_, const std::string& search_ );
	std::vector<std::string> stringSplit( const std::string& input_, const char delim_ );
	std::string randomString( size_t length_ );
	std::string extractStringFromJson( const nlohmann::json& json_ );
	const std::map<std::string, std::string> getSerialPorts();

	bool generateKeys( std::string& public_, std::string& private_ );
	std::string generateHash( const std::string& input_, const std::string& privateKey_ );
	std::string encrypt( const std::string& input_, const std::string& privateKey_ );
	std::string decrypt( const std::string& input_, const std::string& publicKey_ );

#define ENUM_UTIL_BASE(E) \
typedef std::underlying_type<E>::type E ## _t; \
friend inline E operator|( E a_, E b_ )    { return static_cast<E>( static_cast<E ## _t>( a_ ) | static_cast<E ## _t>( b_ ) ); }; \
friend inline E operator&( E a_, E b_ )    { return static_cast<E>( static_cast<E ## _t>( a_ ) & static_cast<E ## _t>( b_ ) ); }; \
friend inline E operator^( E a_, E b_ )    { return static_cast<E>( static_cast<E ## _t>( a_ ) ^ static_cast<E ## _t>( b_ ) ); }; \
friend inline E operator~( E a_ )          { return static_cast<E>( ~static_cast<E ## _t>( a_ ) ); }; \
friend inline E& operator|=( E& a_, E b_ ) { a_ = a_ | b_; return a_; }; \
friend inline E& operator&=( E& a_, E b_ ) { a_ = a_ & b_; return a_; }; \
friend inline E& operator^=( E& a_, E b_ ) { a_ = a_ ^ b_; return a_; };

#define ENUM_UTIL_W_TEXT(E,S) \
ENUM_UTIL_BASE(E) \
static inline std::string resolve ## E( const E& enum_ ) { \
	return S.at( enum_ ); \
}; \
static inline E resolve ## E( const std::string& enum_ ) { \
	for ( auto textIt = S.begin(); textIt != S.end(); textIt++ ) { \
		if ( textIt->second == enum_ ) { \
			return textIt->first; \
		} \
	} \
	throw std::invalid_argument( enum_ + " cannot be resolved to " + #E ); \
}; \
friend std::ostream& operator<<( std::ostream& os_, const E& enum_ ) { \
	auto textIt = S.find( enum_ ); \
	if ( textIt != S.end() ) { \
		os_ << textIt->second; \
	} else { \
		os_.setstate( std::ios::failbit ); \
	} \
	return os_; \
}; \
friend std::istream& operator>>( std::istream& is_, E& enum_ ) { \
	std::string value; \
	is_ >> value; \
	for ( auto textIt = S.begin(); textIt != S.end(); textIt++ ) { \
		if ( textIt->second == value ) { \
			enum_ = textIt->first; \
			return is_; \
		} \
	} \
	is_.setstate( std::ios::failbit ); \
	return is_; \
};

#define ENUM_UTIL(E) \
ENUM_UTIL_BASE(E) \
static inline E ## _t resolve ## E( const E& enum_ ) { \
	return static_cast<E ## _t>( enum_ ); \
}; \
static inline E resolve ## E( const E ## _t& enum_ ) { \
	return static_cast<E>( enum_ ); \
}; \
friend std::ostream& operator<<( std::ostream& os_, const E& enum_ ) { \
	os_ << static_cast<E ## _t>( enum_ ); \
	return os_; \
}; \
friend std::istream& operator>>( std::istream& is_, E& enum_ ) { \
	E ## _t value; \
	is_ >> value; \
	enum_ = static_cast<E>( value ); \
	return is_; \
};

}; // namespace micasa
