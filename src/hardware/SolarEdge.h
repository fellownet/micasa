#pragma once

#include "../Hardware.h"
#include "../Network.h"

namespace micasa {

	class SolarEdge final : public Hardware {

	public:
		SolarEdge( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~SolarEdge() { };

		void start() override;
		void stop() override;
		
		const std::string getLabel() const override { return "SolarEdge API"; };
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) override { return true; };

	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;
		
	private:
		void _processHttpReply( mg_connection* connection_, const http_message* message_ );
		
	}; // class SolarEdge

}; // namespace micasa
