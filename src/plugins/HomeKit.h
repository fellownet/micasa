#pragma once

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "../Plugin.h"
#include "../Network.h"

extern "C" {
	#include "srp.h"
}

void micasa_avahi_client_callback( AvahiClient* client_, AvahiClientState state_, void* userdata_ );
void micasa_avahi_group_callback( AvahiEntryGroup* group_, AvahiEntryGroupState state_, void* userdata_ );

namespace micasa {

	// =======
	// HomeKit
	// =======

	class HomeKit final : public Plugin {

		friend void (::micasa_avahi_client_callback)( AvahiClient* client_, AvahiClientState state_, void* userdata_ );
		friend void (::micasa_avahi_group_callback)( AvahiEntryGroup* group_, AvahiEntryGroupState state_, void* userdata_ );

	private: class Session; // forward declaration

	public:
		static const char* label;

		HomeKit( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~HomeKit() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );
		void updateDeviceJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void updateDeviceSettingsJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void putDeviceSettingsJson( std::shared_ptr<Device> device_, const nlohmann::json& json_, bool owned_ ) override;

	private:
		std::shared_ptr<Network::Connection> m_bind;
		char* m_name;
		AvahiClient* m_client;
		AvahiSimplePoll* m_poll;
		AvahiEntryGroup* m_group;
		std::thread m_worker;

		SRP* m_srp;
		cstr* m_publicKey;
		cstr* m_secretKey;
		char m_sessionKey[64]; // unencrypted session key?
		std::map<std::shared_ptr<Network::Connection>, HomeKit::Session> m_sessions;

		void _createService( AvahiClient* client_ );
		bool _processRequest( std::shared_ptr<Network::Connection> connection_ );
		bool _handleIdentify( std::shared_ptr<Network::Connection> connection_ );
		bool _handlePair( std::shared_ptr<Network::Connection> connection_ );
		bool _handlePairVerify( std::shared_ptr<Network::Connection> connection_ );
		bool _handlePairings( std::shared_ptr<Network::Connection> connection_ );
		bool _handleAccessories( std::shared_ptr<Network::Connection> connection_ );
		bool _handleCharacteristics( std::shared_ptr<Network::Connection> connection_ );
		bool _handleResource( std::shared_ptr<Network::Connection> connection_ );

		std::string _getSetupCode() const;

		// =======
		// Session
		// =======

		class Session final {

		public:
			char m_sessionKey[64]; // encrypted session key?
			uint8_t m_controllerToAccessoryKey[32];
			unsigned long long m_controllerToAccessoryCount;
			uint8_t m_accessoryToControllerKey[32];
			unsigned long long m_accessoryToControllerCount;
			std::vector<std::weak_ptr<Device>> m_devices;
			mutable std::mutex m_sessionMutex;

			Session() :
				m_controllerToAccessoryCount( 0 ),
				m_accessoryToControllerCount( 0 )
			{
			}
		};

	}; // class HomeKit

}; // namespace micasa
