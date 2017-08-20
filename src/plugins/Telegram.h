#pragma once

#include "../Plugin.h"
#include "../Network.h"

#define TELEGRAM_BUSY_WAIT_MSEC         60000 // how long to wait for result
#define TELEGRAM_BUSY_BLOCK_MSEC        500   // how long to block activies while waiting for result

namespace micasa {

	class Telegram final : public Plugin {

	public:
		static const char* label;

		Telegram( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~Telegram() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );

	private:
		std::shared_ptr<Scheduler::Task<>> m_task;
		std::shared_ptr<Network::Connection> m_connection;
		std::string m_username;
		int m_lastUpdateId;
		bool m_acceptMode;

		bool _process( const std::string& data_ );

	}; // class PiFace

}; // namespace micasa
