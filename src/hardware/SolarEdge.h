#pragma once

#include "../Hardware.h"

namespace micasa {

	class SolarEdge final : public Hardware {

	public:
		static const constexpr char* label = "SolarEdge API";

		SolarEdge( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~SolarEdge() { };

		void start() override;
		void stop() override;

		std::string getLabel() const throw() override { return SolarEdge::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		nlohmann::json getJson(  bool full_ = false  ) const override;
		nlohmann::json getSettingsJson() const override;

	private:
		void _process( const std::string& body_ );
		
	}; // class SolarEdge

}; // namespace micasa
