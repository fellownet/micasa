#pragma once

#include "../Hardware.h"
#include "../Network.h"

namespace micasa {

	class SolarEdge final : public Hardware {

	public:
		static const char* label;

		SolarEdge( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~SolarEdge() { };

		void start() override;
		void stop() override;

		std::string getLabel() const throw() override { return SolarEdge::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		nlohmann::json getJson(  bool full_ = false  ) const override;
		nlohmann::json getSettingsJson() const override;

	private:
		std::shared_ptr<Scheduler::Task<> > m_task;
		std::shared_ptr<Network::Connection> m_connection;

		bool _process( const std::string& data_ );
		
	}; // class SolarEdge

}; // namespace micasa
