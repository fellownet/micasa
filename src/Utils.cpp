#include "Utils.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <iterator>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#ifdef _WITH_LIBUDEV
	#include <libudev.h>
#endif // _WITH_LIBUDEV

namespace micasa {

	using namespace nlohmann;

	//using BIO_MEM_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;

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
	
	std::vector<std::string> stringSplit( const std::string& input_, const char delim_ ) {
		std::vector<std::string> results;
		std::string token = "";
		std::stringstream stream( input_ );
		while( std::getline( stream, token, delim_ ) ) {
			results.push_back( token );
		}
		return results;
	};
	
	std::string stringJoin( const std::vector<std::string>& input_, const std::string& glue_ ) {
		std::ostringstream result;
		for ( const auto& i : input_ ) {
			if ( &i != &input_[0] ) {
				result << glue_;
			}
			result << i;
		}
		return result.str();
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

	bool validateSettings( const json& input_, json& output_, const json& settings_, std::vector<std::string>* invalid_, std::vector<std::string>* missing_, std::vector<std::string>* errors_ ) {
		bool result = true;

		for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
			auto setting = *settingsIt;
			const std::string& name = setting["name"].get<std::string>();
			const std::string& label = setting["label"].get<std::string>();
			const std::string& type = setting["type"].get<std::string>();

			// Search for the setting name in the input parameters. The name in the defenition should
			// me used as the key when submitting settings.
			auto find = input_.find( name );
			if (
				find != input_.end()
				&& ! (*find).is_null()
				&& (
					! (*find).is_string()
					|| (*find).get<std::string>().size() > 0
				)
			) {

				// Then the submitted value is validated depending on the type defined in the setting.
				try {
					if ( type == "double" ) {

						double value = jsonGet<double>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<double>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<double>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;

					} else if ( type == "boolean" ) {

						bool value = jsonGet<bool>( *find );
						output_[name] = value;

						// If this boolean property was set additional settings might be available which also  need to
						// be  stored in the settings object.
						if (
							value
							&& ( find = setting.find( "settings" ) ) != setting.end()
						) {
							result = result && validateSettings( input_, output_, *find, invalid_, missing_, errors_ );
						}

					} else if ( type == "byte" ) {

						unsigned char value = jsonGet<unsigned char>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;

					} else if ( type == "short" ) {
						
						short value = jsonGet<short>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<int>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<int>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;
					
					} else if ( type == "int" ) {

						int value = jsonGet<int>( *find );
						if (
							( find = setting.find( "minimum" ) ) != setting.end()
							&& value < (*find).get<int>()
						) {
							throw std::runtime_error( "value too low" );
						}
						if (
							( find = setting.find( "maximum" ) ) != setting.end()
							&& value > (*find).get<int>()
						) {
							throw std::runtime_error( "value too high" );
						}
						output_[name] = value;
					
					} else if ( type == "list" ) {

						std::string value = jsonGet<std::string>( *find );

						bool exists = false;
						bool hasSettings = false;
						json::iterator optionsIt;
						for ( optionsIt = setting["options"].begin(); optionsIt != setting["options"].end(); optionsIt++ ) {
							std::string option;
							if ( (*optionsIt)["value"].is_number() ) {
								option = std::to_string( (*optionsIt)["value"].get<int>() );
							} else if ( (*optionsIt)["value"].is_string() ) {
								option = (*optionsIt)["value"].get<std::string>();
							} else {
								continue;
							}
							if ( option == value ) {
								exists = true;
								if ( ( find = (*optionsIt).find( "settings" ) ) != (*optionsIt).end() ) {
									hasSettings = true;
								}
								break;
							}
						}
						if ( ! exists ) {
							throw std::runtime_error( "value not a valid option" );
						}
						output_[name] = (*optionsIt)["value"];
						
						// If a certain list option is selected that has it's own additional settings then these
						// settings need to be stored aswell.
						if ( hasSettings ) {
							result = result && validateSettings( input_, output_, *find, invalid_, missing_, errors_ );
						}

					} else if ( type == "string" ) {

						std::string value = jsonGet<std::string>( *find );
						if (
							( find = setting.find( "maxlength" ) ) != setting.end()
							&& value.size() > (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too long" );
						}
						if (
							( find = setting.find( "minlength" ) ) != setting.end()
							&& value.size() < (*find).get<unsigned int>()
						) {
							throw std::runtime_error( "value too short" );
						}
						output_[name] = value;
					
					} else if ( type == "display" ) {
						/* nothing to validate here */
					} else {
						throw std::logic_error( "invalid type " + type );
					}

				} catch( std::runtime_error exception_ ) { // thrown by jsonGet if conversion fails
					if ( invalid_ ) invalid_->push_back( name );
					if ( errors_ ) errors_->push_back( "invalid value for " + label + " (" + exception_.what() + ")" );
					result = false;
				} catch( ... ) {
					if ( invalid_ ) invalid_->push_back( name );
					result = false;
				}
			
			// The setting is missing from the input. See if the setting is mandatory, which would cause a problem if
			// there's no default specified.
			} else if (
				( find = setting.find( "mandatory" ) ) != setting.end()
				&& (*find).get<bool>()
			) {

				// If there was a default specified than the field will be set to that value and no error is reported.
				if ( ( find = setting.find( "default" ) ) != setting.end() ) {
					output_[name] = *find;
				} else {
					if ( missing_ ) missing_->push_back( name );
					result = false;
				}

			// If the setting was not found and also was not mandatory, a null value is passed along. This can be used
			// to reset a value.
			} else {
				output_[name] = nullptr;
			}
		}

		return result;
	};

	// http://stackoverflow.com/questions/2530096/how-to-find-all-serial-devices-ttys-ttyusb-on-linux-without-opening-them
	// http://www.signal11.us/oss/udev/
	const std::map<std::string, std::string> getSerialPorts() {
		std::map<std::string, std::string> results;

#ifdef _WITH_LIBUDEV
		// Create the udef object which is used to discover tty usb devices.
		udev* udev = udev_new();
		if ( udev ) {

			// Iterate through the list of devices in the tty subsystem.
			udev_enumerate* enumerate = udev_enumerate_new( udev );
			udev_enumerate_add_match_is_initialized( enumerate );
			udev_enumerate_add_match_subsystem( enumerate, "tty" );
			udev_enumerate_scan_devices( enumerate );

			udev_list_entry* devices = udev_enumerate_get_list_entry( enumerate );
			udev_list_entry* dev_list_entry;
			udev_list_entry_foreach( dev_list_entry, devices ) {

				// Extract the full path to the tty device and create a udev device out of it.
				const char* path = udev_list_entry_get_name( dev_list_entry );
				udev_device* dev = udev_device_new_from_syspath( udev, path );

				// Get the device node and see if it's valid. If so, it can be used as an usb device
				// for various hardware. The parent usb device holds information on the device.
				const char* node = udev_device_get_devnode( dev );
				if ( node ) {
					struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype( dev, "usb", "usb_device" );
					if ( parent ) {
						std::stringstream ss;
						ss << node;
						ss << " " << ( udev_device_get_sysattr_value( parent, "idVendor" ) ?: "" );
						ss << " " << ( udev_device_get_sysattr_value( parent, "idProduct" ) ?: "" );
						ss << " " << ( udev_device_get_sysattr_value( parent, "manufacturer" ) ?: "" );
						ss << " " << ( udev_device_get_sysattr_value( parent, "product" ) ?: "" );
						results[node] = ss.str();
					}
				}

				udev_device_unref( dev );
			}

			// Free the enumerator object.
			udev_enumerate_unref( enumerate );
			udev_unref( udev );
		}
#endif // _WITH_LIBUDEV
		
		return results;
	};

/*
	class RSA_ptr {
	public:
		RSA_ptr( RSA* rsa_ ) : _rsa( rsa_ ) { };
		~RSA_ptr() { RSA_free( this->_rsa ); };
		RSA* get() const { return this->_rsa; };
	private:
		RSA* _rsa;
	};

	std::string generateHash( const std::string& input_, const std::string& privateKey_ ) {
		auto targetString = encrypt64( input_, privateKey_ );
		SHA256_CTX context;
		if ( ! SHA256_Init( &context ) ) {
			throw std::runtime_error( "sha256 init failure" );
		}
		unsigned char hash[SHA256_DIGEST_LENGTH];
		if (
			! SHA256_Update( &context, targetString.c_str(), targetString.size() )
			|| ! SHA256_Final( hash, &context )
		) {
			throw std::runtime_error( "sha256 failure" );
		}
		std::stringstream ss;
		for ( int i = 0; i < SHA256_DIGEST_LENGTH; i++ ) {
			ss << std::hex << std::setw( 2 ) << std::setfill( '0' ) << (int)hash[i];
		}
		return ss.str();
	};

	bool generateKeys( std::string& public_, std::string& private_ ) {
		BIGNUM* bignum = BN_new();
		if ( ! BN_set_word( bignum, RSA_F4 ) ) {
			return false;
		}
		RSA_ptr rsa( RSA_new() );
		if ( ! RSA_generate_key_ex( rsa.get(), 2048, bignum, NULL ) ) {
			return false;
		}
		BN_clear_free( bignum );

		BUF_MEM *buff = NULL;

		BIO_MEM_ptr bio( BIO_new( BIO_s_mem() ), ::BIO_free );
		PEM_write_bio_RSAPublicKey( bio.get(), rsa.get() );
		BIO_get_mem_ptr( bio.get(), &buff );
		public_.assign( buff->data, buff->length );

		BIO_MEM_ptr bio2( BIO_new( BIO_s_mem() ), ::BIO_free );
		PEM_write_bio_RSAPrivateKey( bio2.get(), rsa.get(), NULL, NULL, 0, NULL, NULL );
		BIO_get_mem_ptr( bio2.get(), &buff );
		private_.assign( buff->data, buff->length );

		X509* cert = X509_new();
		if ( ! cert ) {
			return false;
		}
		X509_set_version( cert, 2 );
		ASN1_INTEGER_set( X509_get_serialNumber( cert), 1 );
		X509_gmtime_adj( X509_get_notBefore( cert ), 0 );
		X509_gmtime_adj( X509_get_notAfter( cert ), (long)(60 * 60 * 24 * 30 ) );

		X509_NAME* name = X509_get_subject_name( cert );
		X509_NAME_add_entry_by_txt( name, "C", MBSTRING_ASC, (unsigned char*)"NL", -1, -1, 0 );
		X509_NAME_add_entry_by_txt( name, "CN", MBSTRING_ASC, (unsigned char*)"test", -1, -1, 0 );
		X509_NAME_add_entry_by_txt( name, "O", MBSTRING_ASC, (unsigned char*)"Fellownet", -1, -1, 0 );
		X509_set_issuer_name( cert, name );

		EVP_PKEY* pkey = EVP_PKEY_new();
		if ( ! pkey ) {
			return false;
		}
		if ( ! EVP_PKEY_assign_RSA( pkey, rsa.get() ) ) {
			return false;
		}
		if ( ! X509_set_pubkey( cert, pkey ) ) {
			return false;
		}
		if ( ! X509_sign( cert, pkey, EVP_sha1() ) ) {
			return false;
		}

		BIO_MEM_ptr bio3( BIO_new( BIO_s_mem() ), ::BIO_free );
		PEM_write_bio_X509( bio3.get(), cert );
		BIO_get_mem_ptr( bio3.get(), &buff );
		cert_.assign( buff->data, buff->length );

		X509_free( cert );
		EVP_PKEY_free( pkey );
		
		return true;
	};

	std::string encrypt64( const std::string& input_, const std::string& privateKey_ ) {

#ifdef _DEBUG
		// The maximum length of the data to be encrypted depends on the key length of the RSA key. If more data needs
		// to be encrypted, AES-like solutions should be implemented.
		assert( input_.size() < 245 && "Maximum length of string to encrypt using RSA is ~245 bytes." );
#endif // _DEBUG

		// Construct a RSA object from the private key stored as a base64 string.
		BIO_MEM_ptr bio( BIO_new( BIO_s_mem() ), ::BIO_free );
		BIO_puts( bio.get(), privateKey_.c_str() );
		RSA_ptr rsa( PEM_read_bio_RSAPrivateKey( bio.get(), NULL, NULL, NULL ) );
		if ( ! rsa.get() ) {
			throw std::runtime_error( "pem -> rsa failure" );
		}

		// Here the data is encrypted.
		int size = RSA_size( rsa.get() );
		char encrypted[size];
		if ( ! RSA_private_encrypt( input_.size(), (unsigned char*)input_.c_str(), (unsigned char*)encrypted, rsa.get(), RSA_PKCS1_PADDING ) ) {
			const char* error_string = ERR_error_string( ERR_get_error(), NULL );
			throw std::runtime_error( error_string );
		}

		// Then the data is base64 encoded.
		BIO *buff, *b64f;
		b64f = BIO_new( BIO_f_base64() );
		buff = BIO_new( BIO_s_mem() );
		buff = BIO_push( b64f, buff );

		BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
		if ( ! BIO_set_close( buff, BIO_CLOSE ) ) {
			throw std::runtime_error( "bio set close failure" );
		}
		BIO_write(buff, encrypted, size );
		if ( ! BIO_flush( buff ) ) {
			throw std::runtime_error( "bio flush failure" );
		}

		BUF_MEM *ptr;
		BIO_get_mem_ptr( buff, &ptr );
		std::string result;
		result.assign( ptr->data, ptr->length );

		BIO_free_all( buff );

		return result;
	};
	
	std::string decrypt64( const std::string& input_, const std::string& publicKey_ ) {

		// Construct a RSA object from the public key stored as a base64 string.
		BIO_MEM_ptr bio( BIO_new( BIO_s_mem() ), ::BIO_free );
		BIO_puts( bio.get(), publicKey_.c_str() );
		RSA_ptr rsa( PEM_read_bio_RSAPublicKey( bio.get(), NULL, NULL, NULL ) );
		if ( ! rsa.get() ) {
			throw std::runtime_error( "pem -> rsa failure" );
		}

		// First the data is base64 decoded.
		char decoded[input_.size()];
		BIO *buff, *b64f;
		b64f = BIO_new( BIO_f_base64() );
		buff = BIO_new_mem_buf( (void *)input_.c_str(), input_.size() );
		buff = BIO_push( b64f, buff );
		
		BIO_set_flags( buff, BIO_FLAGS_BASE64_NO_NL );
		if ( ! BIO_set_close( buff, BIO_CLOSE ) ) {
			throw std::runtime_error( "bio set close failure" );
		}
		int size1 = BIO_read( buff, decoded, input_.size() );
		BIO_free_all( buff );

		// Then the data is decrypted.
		int size2 = RSA_size( rsa.get() );
		char decrypted[size2];
		size2 = RSA_public_decrypt( size1, (unsigned char*)decoded, (unsigned char*)decrypted, rsa.get(), RSA_PKCS1_PADDING );
		if ( ! size2 ) {
			throw std::runtime_error( "rsa decrypt failure" );
		}

		std::string result( decrypted, size2 );
		return result;
	};
*/

} // namespace micasa
