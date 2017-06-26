#pragma once

#include <chrono>

#include "../Plugin.h"
#include "../Network.h"

namespace micasa {

	class WeatherUnderground final : public Plugin {

	public:
		static const char* label;

		WeatherUnderground( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~WeatherUnderground() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override { return true; };
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );

	private:
		std::shared_ptr<Network::Connection> m_connection;
		std::string m_details;

		void _process( const std::string& data_ );

	}; // class WeatherUnderground

}; // namespace micasa
