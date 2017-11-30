#pragma once

#include <list>

#include "ZWave.h"

#include "../Plugin.h"

#include "Notification.h"

#define OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC              8000 // how long to wait for result
#define OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC             1000 // how long to block node while waiting for result
#define OPEN_ZWAVE_NODE_RACE_WAIT_MSEC              1500

namespace micasa {

	class ZWaveNode final : public Plugin {

		friend class ZWave;

	public:
		static const char* label;

		ZWaveNode( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~ZWaveNode() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		void putSettingsJson( const nlohmann::json& settings_ ) override;
		void updateDeviceJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void updateDeviceSettingsJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;

	private:
		unsigned int m_homeId;
		unsigned int m_nodeId;
		nlohmann::json m_configuration;
		mutable std::mutex m_configurationMutex;

		void _handleNotification( const OpenZWave::Notification* notification_ );
		void _processValue( const OpenZWave::ValueID& valueId_ );
		void _updateNames();

	}; // class ZWaveNode

}; // namespace micasa
