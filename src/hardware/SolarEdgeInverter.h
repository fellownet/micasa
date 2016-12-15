#pragma once

#include "../Hardware.h"
#include "../Network.h"

namespace micasa {
	
	class SolarEdgeInverter final : public Hardware {
		
	public:
		SolarEdgeInverter( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Hardware( id_, reference_, parent_, label_ ) { };
		~SolarEdgeInverter() { };
		
		void start() override;
		void stop() override;
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };
		
	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );
		
	private:
		bool m_first = true;
		
		void _processHttpReply( mg_connection* connection_, const http_message* message_ );
		
	}; // class SolarEdgeInverter
	
}; // namespace micasa
