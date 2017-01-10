#include "Utils.h"

#include <algorithm>
#include <sstream>
#include <iostream>

#ifdef _WITH_LIBUDEV
#include <libudev.h>
#endif // _WITH_LIBUDEV

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
	
	std::vector<std::string> stringSplit( const std::string& input_, const char& delim_ ) {
		std::vector<std::string> results;
		stringSplit( input_, delim_, results );
		return results;
	};
	
	void stringSplit( const std::string& input_, const char& delim_, std::vector<std::string>& results_ ) {
		std::string token = "";
		std::stringstream stream( input_ );
		while( std::getline( stream, token, delim_ ) ) {
			results_.push_back( token );
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
	
	std::string extractStringFromJson( const json& json_ ) {
		if ( json_.is_string() ) {
			return json_.get<std::string>();
		} else if ( json_.is_number() ) {
			return std::to_string( json_.get<int>() );
		} else if ( json_.is_boolean() ) {
			return json_.get<bool>() ? "1" : "0";
		}
		return "";
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

	std::map<std::string, std::string> extractSettingsFromJson( const json& data_ ) {
		std::map<std::string, std::string> settings;
		if (
			data_.find( "settings" ) != data_.end()
			&& data_["settings"].is_array()
		) {
			for ( auto settingsIt = data_["settings"].begin(); settingsIt != data_["settings"].end(); settingsIt++ ) {
				try {
					if (
						(*settingsIt).is_object()
						&& (*settingsIt).find( "name" ) != (*settingsIt).end()
						&& (*settingsIt).find( "value" ) != (*settingsIt).end()
					) {
						settings[(*settingsIt)["name"]] = extractStringFromJson( (*settingsIt)["value"] );
					}
				} catch( ... ) { }
			}
		}
		return settings;
	};
	
} // namespace micasa
