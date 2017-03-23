#pragma once

#include <list>

#include "ZWave.h"

#include "../Hardware.h"

#include "Notification.h"

#define OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC              30000 // how long to wait for result
#define OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC             5000  // how long to block node while waiting for result
#define OPEN_ZWAVE_NODE_RACE_WAIT_MSEC              3000
#define OPEN_ZWAVE_NODE_DUPLICATE_VALUE_FILTER_MSEC 250 // how long to block duplicate values at the value id level

namespace micasa {

	class ZWaveNode final : public Hardware {

		friend class ZWave;

	public:
		static const constexpr char* label = "Z-Wave Node";

		ZWaveNode( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~ZWaveNode() { };

		void start() override;
		void stop() override;

		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		void putSettingsJson( nlohmann::json& settings_ ) override;
		nlohmann::json getDeviceJson( std::shared_ptr<const Device> device_, bool full_ = false ) const override;
		nlohmann::json getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const override;

	private:
		unsigned int m_homeId = 0;
		unsigned int m_nodeId = 0;
		nlohmann::json m_configuration = nlohmann::json::object();
		mutable std::mutex m_configurationMutex;

		void _handleNotification( const OpenZWave::Notification* notification_ );
		void _processValue( const OpenZWave::ValueID& valueId_, Device::UpdateSource source_ );
		void _updateNames();

	}; // class ZWaveNode

}; // namespace micasa
