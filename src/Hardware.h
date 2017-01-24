#pragma once

#include <map>
#include <mutex>
#include <vector>
#include <memory>

#include "Logger.h"
#include "WebServer.h"
#include "Device.h"
#include "Settings.h"
#include "Settings.h"

namespace micasa {

	using namespace nlohmann;

	class Hardware : public Worker, public std::enable_shared_from_this<Hardware> {

	public:
		enum class Type: unsigned short {
			HARMONY_HUB = 1,
#ifdef _WITH_OPENZWAVE
			ZWAVE,
			ZWAVE_NODE,
#endif // _WITH_OPENZWAVE
			P1_METER,
			PIFACE,
			PIFACE_BOARD,
			RFXCOM,
			SOLAREDGE,
			SOLAREDGE_INVERTER,
			WEATHER_UNDERGROUND,
			DUMMY
		}; // enum Type
		static const std::map<Type, std::string> TypeText;
		ENUM_UTIL_W_TEXT( Type, TypeText );

		enum State {
			DISABLED = 1,
			INIT,
			READY,
			FAILED,
			SLEEPING,
			DISCONNECTED
		}; // enum State
		static const std::map<State, std::string> StateText;
		ENUM_UTIL_W_TEXT( State, StateText );

		struct PendingUpdate {
		public:
			PendingUpdate( Device::UpdateSource source_ ) : source( source_ ) { };

			std::timed_mutex updateMutex;
			std::condition_variable condition;
			std::mutex conditionMutex;
			bool done = false;
			Device::UpdateSource source;
		}; // struct PendingUpdate

		static const constexpr char* settingsName = "hardware";

		virtual ~Hardware();
		friend std::ostream& operator<<( std::ostream& out_, const Hardware* hardware_ ) { out_ << hardware_->getName(); return out_; }

		// This is the preferred way of creating a hardware instance of a specific type. Hence the protected
		// constructor.
		static std::shared_ptr<Hardware> factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );

		virtual void start();
		virtual void stop();

		unsigned int getId() const throw() { return this->m_id; };
		Type getType() const throw() { return this->m_type; };
		State getState() const throw() { return this->m_state; };
		void setState( const State& state_ );
		std::string getReference() const throw() { return this->m_reference; };
		std::string getName() const;
		std::shared_ptr<Settings<Hardware> > getSettings() const throw() { return this->m_settings; };
		std::shared_ptr<Hardware> getParent() const throw() { return this->m_parent; };
		bool needsRestart() const throw() { return this->m_needsRestart; };
		std::shared_ptr<Device> getDevice( const std::string& reference_ ) const;
		std::shared_ptr<Device> getDeviceById( const unsigned int& id_ ) const;
		std::shared_ptr<Device> getDeviceByName( const std::string& name_ ) const;
		std::shared_ptr<Device> getDeviceByLabel( const std::string& label_ ) const;
		std::vector<std::shared_ptr<Device> > getAllDevices() const;
		void removeDevice( const std::shared_ptr<Device> device_ );

		virtual json getJson( bool full_ = false ) const;
		virtual std::string getLabel() const =0;
		virtual bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) =0;

	protected:
		Hardware( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ );

		const unsigned int m_id;
		const Type m_type;
		const std::string m_reference;
		const std::shared_ptr<Hardware> m_parent;
		std::shared_ptr<Settings<Hardware> > m_settings;
		bool m_needsRestart = false;

		template<class T> std::shared_ptr<T> _declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ = false );

		// The queuePendingUpdate and it's counterpart _releasePendingUpdate methods can be used to queue an
		// update so that subsequent updates are blocked until the update has been confirmed by the hardware.
		// It also makes sure that the source of the update is remembered during this time.
		bool _queuePendingUpdate( const std::string& reference_, const Device::UpdateSource& source_, const unsigned int& blockNewUpdate_ = 3000, const unsigned int& waitForResult_ = 30000 );
		bool _releasePendingUpdate( const std::string& reference_, Device::UpdateSource& source_ );

	private:
		std::vector<std::shared_ptr<Device> > m_devices;
		mutable std::mutex m_devicesMutex;
		std::map<std::string, std::shared_ptr<PendingUpdate> > m_pendingUpdates;
		mutable std::mutex m_pendingUpdatesMutex;
		State m_state = State::DISABLED;

	}; // class Hardware

}; // namespace micasa
