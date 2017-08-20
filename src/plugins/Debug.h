#pragma once

#include "../Plugin.h"

namespace micasa {

	class Debug final : public Plugin {

	public:
		static const char* label;

		Debug( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : Plugin( id_, type_, reference_, parent_ ) { };
		~Debug() { };

		void start() override;
		void stop() override;

		std::string getLabel() const override { return Debug::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;

	}; // class Debug

}; // namespace micasa
