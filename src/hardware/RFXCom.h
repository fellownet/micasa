#pragma once

#include "../Hardware.h"

// BYTE is defined in WinDef.h and is used in RFXtrx.h, so we're using it here aswell.
typedef unsigned char BYTE;

#include "RFXtrx.h"

#define RFXCOM_BUSY_WAIT_MSEC  30000 // how long to wait for result
#define RFXCOM_BUSY_BLOCK_MSEC 3000  // how long to block hardware while waiting for result

#define RFXCOM_MAX_PACKET_SIZE 40

#define RFXCOM_DEVICE_SETTING_CUSTOM "_rfxcom_custom"

namespace micasa {

	class Serial;

	class RFXCom final : public Hardware {

	public:
		static const char* label;

		RFXCom( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );
		~RFXCom() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override { return RFXCom::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;
		nlohmann::json getDeviceJson( std::shared_ptr<const Device> device_, bool full_ = false ) const override;
		nlohmann::json getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const override;

	private:
		std::shared_ptr<Serial> m_serial;
		unsigned int m_packetPosition;
		unsigned char m_packet[RFXCOM_MAX_PACKET_SIZE];

		bool _processPacket();
		bool _handleTempHumPacket( const tRBUF* packet_ );
		bool _handleLightning2Packet( const tRBUF* packet_ );
		void _installResourceHandlers();

	}; // class RFXCom

}; // namespace micasa
