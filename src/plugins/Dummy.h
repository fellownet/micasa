#pragma once

#include "../Plugin.h"

namespace micasa {

	class Dummy final : public Plugin {

	public:
		static const char* label;

		Dummy( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : Plugin( id_, type_, reference_, parent_ ) { };
		~Dummy() { };

		void start() override;
		void stop() override;

		std::string getLabel() const override { return Dummy::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;

	}; // class Dummy

}; // namespace micasa
