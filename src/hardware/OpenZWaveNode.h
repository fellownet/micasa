#pragma once

#ifdef WITH_OPENZWAVE

#include <list>

#include "../Hardware.h"
#include "OpenZWave.h"

#include "Notification.h"

#define OPEN_ZWAVE_NODE_BUSY_WAIT_MSEC		30000 // how long to wait for result
#define OPEN_ZWAVE_NODE_BUSY_BLOCK_MSEC		3000 // how long to block node while waiting for result

#define OPENZWAVE_NODE_SETTING_CONFIGURATION "_ozw_configuration"

namespace micasa {

	using namespace nlohmann;
	
	class OpenZWaveNode final : public Hardware {

		friend class OpenZWave;
		
	public:
		enum State {
			STARTING = 1,
			READY,
			SLEEPING,
			DEAD
		}; // enum State

		OpenZWaveNode( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ ) : Hardware( id_, reference_, parent_, label_ ) { };
		~OpenZWaveNode() { };
		
		void start() override;
		void stop() override;

		bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ );

	protected:
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ ) { return std::chrono::milliseconds( 1000 * 60 * 5 ); }

	private:
		volatile State m_nodeState = STARTING;
		json m_configuration;
		mutable std::mutex m_configurationMutex;
		
		void _handleNotification( const ::OpenZWave::Notification* notification_ );
		void _processValue( const ::OpenZWave::ValueID& valueId_, unsigned int source_ );
		void _updateNodeInfo( const unsigned int& homeId_, const unsigned char& nodeId_ );
		
	}; // class OpenZWaveNode

}; // namespace micasa

#endif // WITH_OPENZWAVE
