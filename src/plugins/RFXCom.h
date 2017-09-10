#pragma once

#include "../Plugin.h"

// BYTE is defined in WinDef.h and is used in RFXtrx.h, so we're using it here aswell.
typedef unsigned char BYTE;

#include "RFXtrx.h"

#define RFXCOM_BUSY_WAIT_MSEC         15000 // how long to wait for result
#define RFXCOM_BUSY_WAIT_INTERVAL     100   // how long to wait between attempts

#define RFXCOM_MAX_PACKET_SIZE 40

#define RFXCOM_DEVICE_SETTING_CUSTOM "_rfxcom_custom"

namespace micasa {

	class Serial;

	class RFXCom final : public Plugin {

	public:
		static const char* label;

		RFXCom( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~RFXCom() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override { return RFXCom::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );
		void updateDeviceJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void updateDeviceSettingsJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;

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
