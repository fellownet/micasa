#pragma once

#include "../Device.h"

namespace micasa {

	class Counter final : public Device {

	public:
		Counter( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ ) : Device( hardware_, id_, reference_, name_ ) { };

		void start() override;
		std::string toString() const;
		bool updateValue( const Device::UpdateSource source_, const int value_ );
		const int getValue() const { return this->m_value; };
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) override;

		std::chrono::milliseconds _work( const unsigned long int iteration_ );

	private:
		int m_value;

	}; // class Counter

}; // namespace micasa
