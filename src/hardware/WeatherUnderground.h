#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <chrono>

#include "../Hardware.h"
#include "../Worker.h"
#include "../WebServer.h"

namespace micasa {

	class WeatherUnderground final : public Hardware, public Worker {

	public:
		WeatherUnderground( const std::string id_, const std::string reference_, std::string name_ ) : Hardware( id_, reference_, name_ ), Worker() { };
		~WeatherUnderground() { };
		
		std::string toString() const;
		void start() override;
		void stop() override;
		void handleResource( const WebServer::Resource& resource_, int& code_, nlohmann::json& output_ );
		void deviceUpdated( Device::UpdateSource source_, std::shared_ptr<Device> device_ ) { };

	protected:
		std::chrono::milliseconds _work( unsigned long int iteration_ );
		
	}; // class WeatherUnderground

}; // namespace micasa
