#pragma once

#include "../Hardware.h"

namespace micasa {

	class Serial;

	class P1Meter final : public Hardware {

	public:
		static const constexpr char* label = "P1 Meter";

		P1Meter( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~P1Meter() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return P1Meter::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override { return std::chrono::milliseconds( 1000 ); };

	private:
		std::shared_ptr<Serial> m_serial;
		
	}; // class P1Meter

}; // namespace micasa
