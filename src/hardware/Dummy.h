#pragma once

#include "../Hardware.h"

namespace micasa {
	
	class Dummy final : public Hardware {
		
	public:
		Dummy( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~Dummy() { };
		
		void start() override;
		void stop() override;
		
		const std::string getLabel() const { return "Dummy Hardware"; };
		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) { return true; };
		
	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ ) { return std::chrono::milliseconds( 1000 * 60 * 15 ); }
		
	private:
		void _installResourceHandlers();
		
	}; // class Dummy
	
}; // namespace micasa
