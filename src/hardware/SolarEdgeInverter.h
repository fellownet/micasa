#pragma once

#include "../Hardware.h"
#include "../Network.h"

namespace micasa {
	
	class SolarEdgeInverter final : public Hardware {
		
	public:
		static const char* label;

		SolarEdgeInverter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~SolarEdgeInverter() { };
		
		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		
	private:
		std::shared_ptr<Network::Connection> m_connection;
		
		void _process( const std::string& data_ );
		
	}; // class SolarEdgeInverter
	
}; // namespace micasa
