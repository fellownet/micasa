#pragma once

#include "../Plugin.h"
#include "../Network.h"

namespace micasa {

	class SolarEdgeInverter final : public Plugin {

	public:
		static const char* label;

		SolarEdgeInverter( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : Plugin( id_, type_, reference_, parent_ ) { };
		~SolarEdgeInverter() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;

	private:
		std::shared_ptr<Network::Connection> m_connection;

		void _process( const std::string& data_ );

	}; // class SolarEdgeInverter

}; // namespace micasa
