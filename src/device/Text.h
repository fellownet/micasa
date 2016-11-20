#pragma once

#include "../Device.h"

namespace micasa {

	class Text final : public Device {

	public:
		Text( std::shared_ptr<Hardware> hardware_, const std::string id_, const std::string reference_, std::string name_ ) : Device( hardware_, id_, reference_, name_ ) { };

		void start() override;
		std::string toString() const;
		void updateValue( Device::UpdateSource source_, std::string value_ );
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ ) override;
		
		std::chrono::milliseconds _work( unsigned long int iteration_ );

	private:
		std::string m_value;

	}; // class Text

}; // namespace micasa
