#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <chrono>
#include <thread>
#include <memory>

#include "../Hardware.h"
#include "../Worker.h"
#include "../WebServer.h"

namespace micasa {

	class WeatherUnderground final : public Hardware, public Worker {

	public:
		WeatherUnderground( std::string id_, std::string unit_, std::string name_, std::map<std::string, std::string> settings_ );
		~WeatherUnderground();
		
		std::string toString() const;
		void start() override;
		void stop() override;
		bool handleRequest( std::string resource_, WebServerResource::Method method_, std::map<std::string, std::string> &data_ ) { return true; /* not implemented yet */ };

	protected:
		std::chrono::milliseconds _work( unsigned long int iteration_ );

	}; // class WeatherUnderground

}; // namespace micasa
