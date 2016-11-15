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
		WeatherUnderground( std::string id_, std::map<std::string, std::string> settings_ );
		~WeatherUnderground();
		
		std::string toString() const;
		bool start();
		bool stop();
		bool handleRequest( std::string resource_, WebServerResource::Method method_, std::map<std::string, std::string> &data_ ) { return true; /* not implemented yet */ };

	protected:
		std::chrono::milliseconds _doWork();

	}; // class WeatherUnderground

}; // namespace micasa
