#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>
#include <iomanip>

#include "json.hpp"

namespace micasa {

	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, const bool strict_, std::string& result_ );
	bool stringIsolate( const std::string& haystack_, const std::string& start_, const std::string& end_, std::string& result_ );
	bool stringStartsWith( const std::string& haystack_, const std::string& search_ );
	std::vector<std::string> stringSplit( const std::string& input_, const char delim_ );
	std::string stringJoin( const std::vector<std::string>& input_, const std::string& glue_ );
	std::string stringFormat( const std::string format_, ... );
	std::string randomString( size_t length_, std::string charset_ = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" );
	int randomNumber( int min_, int max_ );

	const std::map<std::string, std::string> getSerialPorts();

	template<typename T> inline T jsonGetImpl( const nlohmann::basic_json<>::value_type& input_, T* ) {
		T value;
		if ( input_.is_string() ) {
			std::istringstream( input_.get<std::string>() ) >> std::fixed >> std::setprecision( 3 ) >> value;
		} else if ( input_.is_number_float() ) {
			std::istringstream( std::to_string( input_.get<double>() ) ) >> std::fixed >> std::setprecision( 3 ) >> value;
		} else if ( input_.is_number() ) {
			std::istringstream( std::to_string( input_.get<long>() ) ) >> value;
		} else if ( input_.is_boolean() ) {
			std::istringstream( input_.get<bool>() ? "1" : "0" ) >> value;
		} else {
			throw std::runtime_error( "invalid json scalar" );
		}
		return value;
	};

	template<typename T> inline std::vector<T> jsonGetImpl( const nlohmann::basic_json<>::value_type& input_, std::vector<T>* ) {
		if ( input_.is_array() ) {
			std::vector<T> value;
			for ( auto &el : input_ ) {
				value.push_back( jsonGetImpl( el, static_cast<T*>( 0 ) ) );
			}
			return value;
		} else {
			throw std::runtime_error( "invalid json array" );
		}
	};

	inline std::string jsonGetImpl( const nlohmann::basic_json<>::value_type& input_, std::string* ) {
		if ( input_.is_string() ) {
			return input_.get<std::string>();
		} else if ( input_.is_number_float() ) {
			return std::to_string( input_.get<double>() );
		} else if ( input_.is_number() ) {
			return std::to_string( input_.get<long>() );
		} else if ( input_.is_boolean() ) {
			return input_.get<bool>() ? "true" : "false";
		} else if ( input_.is_array() ) {
			return stringJoin( jsonGetImpl( input_, static_cast<std::vector<std::string>*>( 0 ) ), "," );
		} else {
			throw std::runtime_error( "invalid json string" );
		}
	};

	inline bool jsonGetImpl( const nlohmann::basic_json<>::value_type& input_, bool* ) {
		if ( input_.is_string() ) {
			return ( input_.get<std::string>() == "1" || input_.get<std::string>() == "true" || input_.get<std::string>() == "yes" );
		} else if ( input_.is_number_float() ) {
			return input_.get<double>() > 0;
		} else if ( input_.is_number() ) {
			return input_.get<long>() > 0;
		} else if ( input_.is_boolean() ) {
			return input_.get<bool>();
		} else {
			throw std::runtime_error( "invalid json bool" );
		}
	};

	inline nlohmann::json jsonGetImpl( const nlohmann::basic_json<>::value_type& input_, nlohmann::json* ) {
		if (
			input_.is_array()
			|| input_.is_object()
		) {
			return input_;
		} else {
			throw std::runtime_error( "invalid json array or object" );
		}
	};

	template<typename T = std::string> inline T jsonGet( const nlohmann::basic_json<>::value_type& input_ ) {
		return jsonGetImpl( input_, static_cast<T*>( 0 ) );
	};
	template<typename T = std::string> inline T jsonGet( const nlohmann::json& input_, const std::string& key_ ) {
		auto find = input_.find( key_ );
		if ( find != input_.end() ) {
			return jsonGetImpl( *find, static_cast<T*>( 0 ) );
		} else {
			throw std::runtime_error( "invalid json key" );
		}
	};

	// =======
	// Defines
	// =======

	#define __likely(expression)   __builtin_expect(!!(expression), 1)
	#define __unlikely(expression) __builtin_expect(!!(expression), 0)

	#define ENUM_UTIL_BASE(E) \
	typedef typename std::underlying_type<E>::type E ## _t; \
	friend constexpr inline E operator|( E a_, E b_ )    { return static_cast<E>( static_cast<E ## _t>( a_ ) | static_cast<E ## _t>( b_ ) ); }; \
	friend constexpr inline E operator&( E a_, E b_ )    { return static_cast<E>( static_cast<E ## _t>( a_ ) & static_cast<E ## _t>( b_ ) ); }; \
	friend constexpr inline E operator^( E a_, E b_ )    { return static_cast<E>( static_cast<E ## _t>( a_ ) ^ static_cast<E ## _t>( b_ ) ); }; \
	friend constexpr inline E operator~( E a_ )          { return static_cast<E>( ~static_cast<E ## _t>( a_ ) ); }; \
	friend inline E& operator|=( E& a_, E b_ ) { a_ = a_ | b_; return a_; }; \
	friend inline E& operator&=( E& a_, E b_ ) { a_ = a_ & b_; return a_; }; \
	friend inline E& operator^=( E& a_, E b_ ) { a_ = a_ ^ b_; return a_; };

	#define ENUM_UTIL_W_TEXT(E,S) \
	ENUM_UTIL_BASE(E) \
	static constexpr inline E ## _t resolve ## E( const E& enum_ ) { \
		return static_cast<E ## _t>( enum_ ); \
	}; \
	static constexpr inline E resolve ## E( const E ## _t& enum_ ) { \
		return static_cast<E>( enum_ ); \
	}; \
	static inline std::string resolveText ## E( const E& enum_ ) { \
		return S.at( enum_ ); \
	}; \
	static inline E resolveText ## E( const std::string& enum_ ) { \
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
	static constexpr inline E ## _t resolve ## E( const E& enum_ ) { \
		return static_cast<E ## _t>( enum_ ); \
	}; \
	static constexpr inline E resolve ## E( const E ## _t& enum_ ) { \
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
