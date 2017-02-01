#pragma once

#include <list>

#include "../Hardware.h"
#include "ZWave.h"

#include "Notification.h"

#define OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC  30000 // how long to wait for result
#define OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC 3000  // how long to block node while waiting for result

namespace micasa {

	class ZWaveNode final : public Hardware {

		friend class ZWave;

	public:
		ZWaveNode( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~ZWaveNode() { };

		void start() override;
		void stop() override;

		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

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
