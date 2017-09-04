#pragma once

#ifdef _DARWIN
	#include <dns_sd.h>
#else
	#include <avahi-client/client.h>
	#include <avahi-client/publish.h>
	#include <avahi-common/simple-watch.h>
#endif // _DARWIN

extern "C" {
	#include "srp.h"

#ifndef _DARWIN
	void micasa_avahi_client_callback( AvahiClient* client_, AvahiClientState state_, void* userdata_ );
	void micasa_avahi_group_callback( AvahiEntryGroup* group_, AvahiEntryGroupState state_, void* userdata_ );
#endif // _DARWIN
}

#include "../Plugin.h"
#include "../Network.h"

namespace micasa {

	// =======
	// HomeKit
	// =======

	class HomeKit final : public Plugin {

#ifndef _DARWIN
		friend void (::micasa_avahi_client_callback)( AvahiClient* client_, AvahiClientState state_, void* userdata_ );
		friend void (::micasa_avahi_group_callback)( AvahiEntryGroup* group_, AvahiEntryGroupState state_, void* userdata_ );
#endif // _DARWIN

	private: class Session;

	public:
		static const char* label;

		HomeKit( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ );
		~HomeKit();

		void start() override;
		void stop() override;
		std::string getLabel() const override { return HomeKit::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool owned_, bool& apply_ ) override;
		nlohmann::json getJson() const override;
		nlohmann::json getSettingsJson() const override;
		static nlohmann::json getEmptySettingsJson( bool advanced_ = false );
		void updateDeviceJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void updateDeviceSettingsJson( std::shared_ptr<const Device> device_, nlohmann::json& json_, bool owned_ ) const override;
		void putDeviceSettingsJson( std::shared_ptr<Device> device_, const nlohmann::json& json_, bool owned_ ) override;
		void beforeRemoveDevice( const std::shared_ptr<Device> device_ ) override;

	private:
		std::shared_ptr<Network::Connection> m_bind;
#ifdef _DARWIN
		_DNSServiceRef_t* m_dnssd;
#else
		char* m_name;
		AvahiClient* m_client;
		AvahiSimplePoll* m_poll;
		AvahiEntryGroup* m_group;
		std::thread m_worker;
#endif // _DARWIN
		SRP* m_srp;
		cstr* m_publicKey;
		cstr* m_secretKey;
		char m_sessionKey[64];
		std::map<std::shared_ptr<Network::Connection>, HomeKit::Session> m_sessions;
		mutable std::mutex m_sessionsMutex;

		void _increaseConfig( bool createService_ );
#ifdef _DARWIN
		void _createService();
#else
		void _createService( AvahiClient* client_ );
#endif // _DARWIN

		void _processRequest( Session& session_ );
		void _handleIdentify( Session& session_ );
		void _handlePair( Session& session_ );
		void _handlePairVerify( Session& session_ );
		void _handlePairings( Session& session_ );
		void _handleAccessories( Session& session_ );
		void _handleCharacteristics( Session& session_ );

		void _addHAPValue( std::shared_ptr<Device> device_, const std::string& format_, nlohmann::json& object_ ) throw( std::runtime_error );

		std::string _getSetupCode() const;
		std::string _getAccessoryId() const;

		// =======
		// Session
		// =======

		class Session final {

		public:
			bool m_encrypted;
			std::shared_ptr<Network::Connection> m_connection;
			std::vector<std::weak_ptr<Device>> m_devices;
			mutable std::mutex m_sessionMutex;

			char m_sessionKey[64];
			uint8_t m_controllerToAccessoryKey[32];
			unsigned long long m_controllerToAccessoryCount;
			uint8_t m_accessoryToControllerKey[32];
			unsigned long long m_accessoryToControllerCount;
			struct http_message m_http;

			Session( std::shared_ptr<Network::Connection> connection_ ) :
				m_encrypted( false ),
				m_connection( connection_ ),
				m_controllerToAccessoryCount( 0 ),
				m_accessoryToControllerCount( 0 )
			{
			};

			Session( const Session& ) = delete; // do not copy
			Session& operator=( const Session& ) = delete; // do not copy-assign
			Session( const Session&& ) = delete; // do not move
			Session& operator=( Session&& ) = delete; // do not move-assign

			void send( std::string protocol_, std::string type_, std::string data_ );
		};

	}; // class HomeKit

}; // namespace micasa
