#pragma once

#include "../Hardware.h"

namespace micasa {
	
	class SolarEdgeInverter final : public Hardware {
		
	public:
		static const constexpr char* label = "SolarEdge Inverter";

		SolarEdgeInverter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~SolarEdgeInverter() { };
		
		void start() override;
		void stop() override;
		
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;
		
	private:
		bool m_first = true;
		
		void _processHttpReply( const std::string& body_ );
		
	}; // class SolarEdgeInverter
	
}; // namespace micasa
