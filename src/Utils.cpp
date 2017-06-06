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

	std::string stringFormat( const std::string& format_, ... ) {
		int size = 100;
		std::string str;
		va_list ap;
		while (1) {
			str.resize( size );
			va_start( ap, format_ );
			int n = vsnprintf( &str[0], size, format_.c_str(), ap );
			va_end( ap );
			if (
				n > -1
				&& n < size
			) {
				return str;
			}
			if ( n > -1 ) {
				size = n + 1;
			} else {
				size *= 2;
			}
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

						unsigned int value = jsonGet<unsigned int>( *find );
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

} // namespace micasa
