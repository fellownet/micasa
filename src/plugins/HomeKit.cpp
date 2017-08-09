// https://github.com/rampantpixels/mdns
// https://en.wikipedia.org/wiki/Multicast_DNS
// - The most recent (dev branch) mongoose works with ipv6 broadcasts
// https://github.com/etwmc/Personal-HomeKit-HAP

#include "../Logger.h"
#include "../Network.h"
#include "../User.h"
#include "../device/Switch.h"

#include "HomeKit.h"

#define MDNS_INVALID_POS ((size_t)-1)

static size_t
mdns_string_find(const char* str, size_t length, char c, size_t offset) {
	const void* found;
	if (offset >= length)
		return MDNS_INVALID_POS;
	found = memchr(str + offset, c, length - offset);
	if (found)
		return (size_t)((const char*)found - str);
	return MDNS_INVALID_POS;
}

static void*
mdns_string_make(void* data, size_t capacity, const char* name, size_t length) {
	size_t pos = 0;
	size_t last_pos = 0;
	size_t remain = capacity;
	unsigned char* dest = (unsigned char*)data;
	while ((last_pos < length) && ((pos = mdns_string_find(name, length, '.', last_pos)) != MDNS_INVALID_POS)) {
		size_t sublength = pos - last_pos;
		if (sublength < remain) {
			*dest = (unsigned char)sublength;
			memcpy(dest + 1, name + last_pos, sublength);
			dest += sublength + 1;
			remain -= sublength + 1;
		}
		else {
			return 0;
		}
		last_pos = pos + 1;
	}
	if (last_pos < length) {
		size_t sublength = length - last_pos;
		if (sublength < capacity) {
			*dest = (unsigned char)sublength;
			memcpy(dest + 1, name + last_pos, sublength);
			dest += sublength + 1;
			remain -= sublength + 1;
		}
		else {
			return 0;
		}
	}
	if (!remain)
		return 0;
	*dest++ = 0;
	return dest;
}



namespace micasa {

	using namespace nlohmann;

	const char* HomeKit::label = "HomeKit";

	HomeKit::HomeKit( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) :
		Plugin( id_, type_, reference_, parent_ )
	{
	};

	void HomeKit::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();

		// First we start listening for mDNS requests. The controller (like an iPhone or Apple TV) will respond to the
		// broadcast which we issue later on, and we need to respond with DNS-like data for it to find us.
		this->m_bind = Network::bind( "udp://[::]:5353", [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
			switch( event_ ) {
				case Network::Connection::Event::DATA: {
					//const std::string& data = connection_->popData();
					//std::cout << data << "\n";
					Logger::log( Logger::LogLevel::WARNING, this, "Connection data received." );
					break;
				}
				default: { break; }
			}
		} );
		if ( ! this->m_bind ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Unable to bind to port 5353." );
			this->setState( Plugin::State::FAILED );
		}

		// Then we're broadcasting an mDNS homekit service periodically.
		this->m_scheduler.schedule( 1000, SCHEDULER_INTERVAL_1HOUR, SCHEDULER_INFINITE, this, [this]( std::shared_ptr<Scheduler::Task<>> ) mutable {
			auto handler = [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
				switch( event_ ) {
					case Network::Connection::Event::CONNECT: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );


size_t capacity = 2048;
void* buffer = 0;
buffer = malloc(capacity);

uint16_t* data = (uint16_t*)buffer;
	//Transaction ID
	*data++ = htons(1);
	//Flags
	*data++ = 0;
	//Questions
	*data++ = htons(1);
	//No answer, authority or additional RRs
	*data++ = 0;
	*data++ = 0;
	*data++ = 0;
	//Name string
	data = (uint16_t*)mdns_string_make(data, capacity - 17, "_hap._tcp.local.", 16 );
	if (!data)
		return;
	//Record type
	*data++ = htons(12);
	//! Unicast response, class IN
	*data++ = htons(0x8000U | 1);


int poe = (char*)data - (char*)buffer;
std::string wat = std::string( (const char*)data, poe );

//std::cout << wat << "\n";


//						std::cout << "SENDING\n";
						connection_->send( wat );




						connection_->close();
						break;
					}
					case Network::Connection::Event::FAILURE: {
						Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
						break;
					}
					case Network::Connection::Event::DATA: {
						Logger::log( Logger::LogLevel::WARNING, this, "Connection data." );
						break;
					}
					case Network::Connection::Event::DROPPED: {
						Logger::log( Logger::LogLevel::WARNING, this, "Connection dropped." );
						break;
					}
					case Network::Connection::Event::CLOSE: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connection closed." );
						break;
					}
					default: { break; }
				}
			};

//			Network::connect( "udp://255.255.255.255:5353", [handler]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
			Network::connect( "udp://224.0.0.251:5353", [handler]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
				handler( connection_, event_ );
			} );
#ifdef _IPV6_ENABLED
//			Network::connect( "udp://[ff02::]:5353", [handler]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
			Network::connect( "udp://[ff02::fb]:5353", [handler]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) -> void {
				handler( connection_, event_ );
			} );
#endif // _IPV6_ENABLED
		} );
	};

	void HomeKit::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		if ( this->m_bind ) {
			this->m_bind->terminate();
		}
		Plugin::stop();
	};

	std::string HomeKit::getLabel() const {
		return HomeKit::label;
	};

	bool HomeKit::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		return false;
	};

	json HomeKit::getJson() const {
		json result = Plugin::getJson();
		result["code"] = "938-58-124"; // format XXX-XX-XXX, X = 0-9, dashes are mandatory
		return result;
	};

	json HomeKit::getSettingsJson() const {
		json result = Plugin::getSettingsJson();
		for ( auto &&setting : HomeKit::getEmptySettingsJson( true ) ) {
			result.push_back( setting );
		}
		return result;
	};

	json HomeKit::getEmptySettingsJson( bool advanced_ ) {
		json result = json::array();
		return result;
	};

}; // namespace micasa
