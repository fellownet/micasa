#pragma once

#include <string>

#include "../Device.h"

namespace micasa {

	class Switch final : public Device {

	public:
		enum Options {
			ON = 1,
			OFF = 2,
			OPEN = 4,
			CLOSED = 8,
			STOPPED = 16,
			STARTED = 32,
		}; // enum Options

		static const std::map<int, std::string> OptionsText;
		
		Switch( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ ) : Device( hardware_, id_, reference_, name_ ) { };
		
		void start() override;
		bool updateValue( const Device::UpdateSource source_, const Options value_ );
		const unsigned int getValue() const { return this->m_value; };
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) override;
		
		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		unsigned int m_value;
		
	}; // class Switch

}; // namespace micasa
