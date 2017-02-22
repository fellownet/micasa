#pragma once

#include "../Hardware.h"

namespace micasa {
	
	class Dummy final : public Hardware {
		
	public:
		static const constexpr char* label = "Dummy Hardware";

		Dummy( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~Dummy() { };
		
		void start() override;
		void stop() override;

		std::string getLabel() const throw() override { return Dummy::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;
	
	}; // class Dummy
	
}; // namespace micasa
