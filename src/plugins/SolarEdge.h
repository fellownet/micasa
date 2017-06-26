#pragma once

#include "../Plugin.h"
#include "../Network.h"

namespace micasa {

	class SolarEdge final : public Plugin {

	public:
		static const char* label;

		SolarEdge( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : Plugin( id_, type_, reference_, parent_ ) { };
		~SolarEdge() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override { return SolarEdge::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override { return true; };
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );

	private:
		std::shared_ptr<Scheduler::Task<>> m_task;
		std::shared_ptr<Network::Connection> m_connection;

		bool _process( const std::string& data_ );

	}; // class SolarEdge

}; // namespace micasa
