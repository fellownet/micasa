#pragma once

#include "../Hardware.h"

namespace micasa {

	class Serial;

	class P1Meter final : public Hardware {

	public:
		P1Meter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~P1Meter();

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return "P1 Meter"; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		json getJson( bool full_ = false ) const override;
	
	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) throw() override { return std::chrono::milliseconds( 1000 ); }

	private:
		std::shared_ptr<Serial> m_serial;
		
	}; // class P1Meter

}; // namespace micasa
