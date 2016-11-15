#pragma once

#include <string>
#include <map>
#include <stdexcept>
#include <cstdlib>

extern "C" {

	#include <curl/curl.h>

	size_t micasa_curl_write_data_callback( void *ptr_, size_t size_, size_t nmemb_, void* pInstance_ );
	size_t micasa_curl_read_data_callback( void *ptr_, size_t size_, size_t nmemb_, void* pInstance_ );

} // extern "C"

namespace micasa {

	class CurlException final : public std::runtime_error {

	public:
		CurlException( const std::string &sMessage_ ) : std::runtime_error( sMessage_ ) { }
		CurlException( CURLcode oError_ ) : std::runtime_error( curl_easy_strerror( oError_ ) ) { }

	}; // class Exception

	class Curl final {

	public:
		Curl( const std::string &sUrl_ );
		~Curl();

		typedef std::map<std::string,std::string> headers_t;

		std::string fetch();
		std::string post( const std::string &data, const headers_t &hdrs );
		std::string put( const std::string &data, const headers_t &hdrs );

		bool isHTTPError() const;

		size_t append_data( void* ptr, size_t size, size_t nmemb );
		size_t upload_data( void* ptr, size_t size, size_t nmemb );

	private:
		Curl( const Curl & ); // no compiler-generated copy constructor wanted

		void _setOptions();
		void _setHTTPHeaders( const headers_t &hdrs ) const;

		void _curlPerform();

		CURL *m_handle;
		std::string m_url;
		std::string m_data;
		std::string m_upload_data;

	}; // class Curl

}; // namespace micasa
