#pragma once

#include <map>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <memory>

#include "Device.h"
#include "Settings.h"
#include "Scheduler.h"

namespace micasa {

	class Hardware : public std::enable_shared_from_this<Hardware> {

	public:
		enum class Type: unsigned short {
			HARMONY_HUB = 1,
#ifdef _WITH_OPENZWAVE
			ZWAVE,
			ZWAVE_NODE,
#endif // _WITH_OPENZWAVE
#ifdef _WITH_LINUX_SPI
			PIFACE,
			PIFACE_BOARD,
#endif // _WITH_LINUX_SPI
			RFXCOM,
			SOLAREDGE,
			SOLAREDGE_INVERTER,
			WEATHER_UNDERGROUND,
			DUMMY,
			TELEGRAM
		}; // enum Type
		static const std::map<Type, std::string> TypeText;
		ENUM_UTIL_W_TEXT( Type, TypeText );

		enum class State: unsigned short {
			DISABLED     = 1,
			INIT         = 2,
			FAILED       = 3,
			DISCONNECTED = 4,
			READY        = 100,
			SLEEPING     = 101
		}; // enum State
		static const std::map<State, std::string> StateText;
		ENUM_UTIL_W_TEXT( State, StateText );

		static const char* settingsName;

		Hardware( const Hardware& ) = delete; // Do not copy!
		Hardware& operator=( const Hardware& ) = delete; // Do not copy-assign!
		Hardware( const Hardware&& ) = delete; // do not move
		Hardware& operator=( Hardware&& ) = delete; // do not move-assign
		
		virtual ~Hardware();
		
		friend std::ostream& operator<<( std::ostream& out_, const Hardware* hardware_ ) { out_ << hardware_->getName(); return out_; }

		static std::shared_ptr<Hardware> factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );

		void init();
		virtual void start();
		virtual void stop();
		unsigned int getId() const { return this->m_id; };
		Type getType() const { return this->m_type; };
		State getState() const { return this->m_state; };
		void setState( const State& state_, bool children_ = false );
		std::string getReference() const { return this->m_reference; };
		std::string getName() const;
		std::shared_ptr<Settings<Hardware>> getSettings() const { return this->m_settings; };
		std::shared_ptr<Hardware> getParent() const { return this->m_parent; };
		std::shared_ptr<Device> getDevice( const std::string& reference_ ) const;
		std::shared_ptr<Device> getDeviceById( const unsigned int& id_ ) const;
		std::shared_ptr<Device> getDeviceByName( const std::string& name_ ) const;
		std::shared_ptr<Device> getDeviceByLabel( const std::string& label_ ) const;
		std::vector<std::shared_ptr<Device>> getAllDevices() const;
		std::vector<std::shared_ptr<Device>> getAllDevices( const std::string& prefix_ ) const;
		template<class T> std::shared_ptr<T> declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_ );
		void removeDevice( const std::shared_ptr<Device> device_ );

		virtual nlohmann::json getJson( bool full_ = false ) const;
		virtual nlohmann::json getSettingsJson() const;
		virtual void putSettingsJson( const nlohmann::json& settings_ ) { };
		virtual nlohmann::json getDeviceJson( std::shared_ptr<const Device> device_, bool full_ = false ) const;
		virtual nlohmann::json getDeviceSettingsJson( std::shared_ptr<const Device> device_ ) const;
		virtual void putDeviceSettingsJson( std::shared_ptr<Device> device_, const nlohmann::json& settings_ ) { };
		virtual std::string getLabel() const =0;
		virtual bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) =0;

	protected:
		const unsigned int m_id;
		const Type m_type;
		const std::string m_reference;
		const std::shared_ptr<Hardware> m_parent;
		std::shared_ptr<Settings<Hardware>> m_settings;
		Scheduler m_scheduler;

		Hardware( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );

		// The queuePendingUpdate and it's counterpart _releasePendingUpdate methods can be used to queue an update
		// so that subsequent updates are blocked until the update has been confirmed by the hardware. It also makes
		/// sure that the source of the update is remembered during this time.
		bool _queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ );
		bool _queuePendingUpdate( const std::string& reference_, const std::string& data_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ );
		bool _queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ );
		bool _queuePendingUpdate( const std::string& reference_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ );

		bool _hasPendingUpdate( const std::string& reference_, Device::UpdateSource& source_, std::string& data_, bool release_ = false );
		bool _hasPendingUpdate( const std::string& reference_, std::string& data_, bool release_ = false );
		bool _hasPendingUpdate( const std::string& reference_, Device::UpdateSource& source_, bool release_ = false );
		bool _hasPendingUpdate( const std::string& reference_, bool release_ = false );

		bool _releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_, std::string& data_ );
		bool _releasePendingUpdate( const std::string& reference_, std::string& data_ );
		bool _releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_ );
		bool _releasePendingUpdate( const std::string& reference_ );

	private:
		typedef struct {
			Device::UpdateSource source;
			std::string data;
		} t_pendingUpdate;

		std::unordered_map<std::string, std::shared_ptr<Device>> m_devices;
		mutable std::mutex m_devicesMutex;
		State m_state = State::DISABLED;
		std::map<std::string, std::shared_ptr<Scheduler::Task<t_pendingUpdate>>> m_pendingUpdates;
		mutable std::mutex m_pendingUpdatesMutex;

	}; // class Hardware

}; // namespace micasa
