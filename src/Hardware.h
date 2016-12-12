#pragma once

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

#include <map>
#include <mutex>
#include <vector>
#include <memory>

#include "Logger.h"
#include "WebServer.h"
#include "Device.h"
#include "Settings.h"

namespace micasa {

	class Hardware : public Worker, public std::enable_shared_from_this<Hardware> {
		
		friend class Controller;

	public:
		enum Type {
			HARMONY_HUB = 1,
			OPEN_ZWAVE,
			OPEN_ZWAVE_NODE,
			P1_METER,
			PIFACE,
			PIFACE_BOARD,
			RFXCOM,
			SOLAREDGE,
			SOLAREDGE_INVERTER,
			WEATHER_UNDERGROUND
		}; // enum Type

		struct PendingUpdate {
			std::timed_mutex updateMutex;
			std::condition_variable condition;
			std::mutex conditionMutex;
			bool done = false;
			unsigned int source;
			PendingUpdate( unsigned int source_ ) : source( source_ ) { };
		}; // struct PendingUpdate
		
		Hardware( const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ );
		virtual ~Hardware();
		friend std::ostream& operator<<( std::ostream& out_, const Hardware* hardware_ ) { out_ << hardware_->getName(); return out_; }

		virtual void start();
		virtual void stop();
		
		const unsigned int& getId() const { return this->m_id; };
		const std::string& getReference() const { return this->m_reference; };
		const std::string& getLabel() const { return this->m_label; };
		const std::string getName() const;
		void setLabel( const std::string& label_ );
		Settings& getSettings() { return this->m_settings; };
		const nlohmann::json getJson() const;

		virtual bool updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) =0;

	protected:
		const unsigned int m_id;
		const std::string m_reference;
		const std::shared_ptr<Hardware> m_parent;
		Settings m_settings;

		std::shared_ptr<Device> _getDevice( const std::string reference_ ) const;
		std::shared_ptr<Device> _getDeviceById( const unsigned int id_ ) const;
		std::shared_ptr<Device> _declareDevice( const Device::Type type_, const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_ );
		
		// The queuePendingUpdate and it's counterpart _releasePendingUpdate methods can be used to queue an
		// update so that subsequent updates are blocked until the update has been confirmed by the hardware.
		// It also makes sure that the source of the update is remembered during this time.
		const bool _queuePendingUpdate( const std::string& reference_, const unsigned int& source_, const unsigned int& blockNewUpdate_ = 3000, const unsigned int& waitForResult_ = 30000 );
		const unsigned int _releasePendingUpdate( const std::string& reference_ );

	private:
		std::string m_label;
		std::vector<std::shared_ptr<Device> > m_devices;
		mutable std::mutex m_devicesMutex;
		std::map<std::string, std::shared_ptr<PendingUpdate> > m_pendingUpdates;
		mutable std::mutex m_pendingUpdatesMutex;

		static std::shared_ptr<Hardware> _factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_, std::string label_ );

	}; // class Hardware

}; // namespace micasa
