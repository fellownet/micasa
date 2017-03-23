#pragma once

#include <chrono>

#include "../Hardware.h"

namespace micasa {

	class WeatherUnderground final : public Hardware {

	public:
		static const constexpr char* label = "Weather Underground";

		WeatherUnderground( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~WeatherUnderground() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		
	private:
		std::string m_details;

		void _process( const std::string& body_ );
	
	}; // class WeatherUnderground

}; // namespace micasa
