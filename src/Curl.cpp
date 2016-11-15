#include <cstring>

#include "Curl.h"

extern "C" {

	size_t micasa_curl_write_data_callback( void *ptr_, size_t size_, size_t nmemb_, void* pInstance_ ) {
		micasa::Curl* obj = reinterpret_cast<micasa::Curl*>( pInstance_ );
		return obj->append_data( ptr_, size_, nmemb_ );
	};

	size_t micasa_curl_read_data_callback( void *ptr_, size_t size_, size_t nmemb_, void* pInstance_ ) {
		micasa::Curl* obj = reinterpret_cast<micasa::Curl*>( pInstance_ );
		return obj->upload_data( ptr_, size_, nmemb_ );
	};

}; // extern "C"

namespace micasa {

	Curl::Curl( const std::string &sUrl_ ) {
		curl_global_init( CURL_GLOBAL_ALL );
		m_handle = curl_easy_init();
		if ( m_handle == NULL )
			throw CurlException( "Unable to initialize curl handler" );
		if ( sUrl_.length() == 0 )
			throw CurlException( "URL can't be of zero length" );
		m_url = sUrl_;
	};

	Curl::~Curl() {
		curl_easy_cleanup( m_handle );
	};

	void Curl::_setHTTPHeaders( const headers_t &hdrs ) const {
		if ( ! hdrs.empty() ) {
			struct curl_slist *chunk = NULL;
			for ( headers_t::const_iterator i = hdrs.begin() ; i != hdrs.end() ; ++i ) {
				std::string h( i->first );
				h += ": ";
				h += i->second;
				chunk = curl_slist_append( chunk, h.c_str() );
			}
			CURLcode res = curl_easy_setopt( m_handle, CURLOPT_HTTPHEADER, chunk );
			if ( res != CURLE_OK ) {
				throw CurlException( res );
			}
		}
	};

	bool Curl::isHTTPError() const {
		long httpReturnCode;
		curl_easy_getinfo( m_handle, CURLINFO_RESPONSE_CODE, &httpReturnCode );
		if (
			httpReturnCode < 200
			|| httpReturnCode > 300
		) {
			return true;
		}
		return false;
	};

	std::string Curl::fetch() {
		this->_setOptions();
		this->_curlPerform();
		return m_data;
	};

	std::string Curl::post( const std::string &data, const headers_t &hdrs ) {
		CURLcode res;

		res = curl_easy_setopt(m_handle, CURLOPT_POST, 1);
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		res = curl_easy_setopt(m_handle, CURLOPT_POSTFIELDS, data.c_str());
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		res = curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, data.length());
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		this->_setOptions();

		this->_setHTTPHeaders(hdrs);

		this->_curlPerform();
		return m_data;
	};

	std::string Curl::put( const std::string &data, const headers_t &hdrs ) {
		CURLcode res;

		this->_setOptions();

		res = curl_easy_setopt(m_handle, CURLOPT_UPLOAD, 1);
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		res = curl_easy_setopt(m_handle, CURLOPT_READFUNCTION, micasa_curl_read_data_callback);
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		res = curl_easy_setopt(m_handle, CURLOPT_READDATA, this);
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		m_upload_data = data;

		res = curl_easy_setopt(m_handle, CURLOPT_INFILESIZE, static_cast<long>(data.size()));
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		this->_setHTTPHeaders(hdrs);

		this->_curlPerform();
		return m_data;
	};

	void Curl::_setOptions() {
		CURLcode res;

		//set the url
		res = curl_easy_setopt(m_handle, CURLOPT_URL, m_url.c_str());
		if ( res != CURLE_OK) {
			throw CurlException(res);
		}

		//progress bar is not require
		res = curl_easy_setopt(m_handle, CURLOPT_NOPROGRESS, 1L);
		if ( res != CURLE_OK ) {
			throw CurlException(res);
		}

		res = curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, 0);
		if ( res != CURLE_OK ) {
			throw CurlException(res);
		}

		// TODO: 0 is 'insecure'. Make it a param in Curl
		res = curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST, 0);
		if ( res != CURLE_OK ) {
			throw CurlException(res);
		}

		//set the callback function
		res = curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, micasa_curl_write_data_callback);
		if ( res != CURLE_OK ) {
			throw CurlException(res);
		}

		//set pointer in call back function
		res = curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, this);
		if ( res != CURLE_OK ) {
			throw CurlException(res);
		}
	};

	void Curl::_curlPerform() {
		CURLcode res;
		res = curl_easy_perform(m_handle);
		if ( res != CURLE_OK ) {
			throw CurlException(res);
		}
	};

	size_t Curl::append_data( void* ptr, size_t size, size_t nmemb ) {
		size_t bytes = size * nmemb;
		const char *s = static_cast<char*>(ptr);
		m_data += std::string(s, s + bytes);
		return bytes;
	};

	size_t Curl::upload_data( void* ptr, size_t size, size_t nmemb ) {
		size_t upload_max = size * nmemb;
		size_t to_send = std::min(m_upload_data.size(), upload_max);
		memcpy(ptr, m_upload_data.data(), to_send);
		m_upload_data.erase(0, to_send);
		return to_send;
	};

}; // namespace micasa
