#include <sstream>

#include "HarmonyHub.h"

#include "json.hpp"

#include "../Network.h"
#include "../device/Switch.h"
#include "../Settings.h"
#include "../User.h"
#include "../Logger.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {

	using namespace nlohmann;

	const char* HarmonyHub::label = "Harmony Hub";

	HarmonyHub::HarmonyHub( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) :
		Hardware( id_, type_, reference_, parent_ ),
		m_connectionState( ConnectionState::IDLE ),
		m_currentActivityId( "-1" ),
		m_received( "" )
	{
	};

	void HarmonyHub::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		this->m_task = this->m_scheduler.schedule( 0, 1, this, "harmony hub", [this]( Scheduler::Task<>& task_ ) {
			if ( ! this->m_settings->contains( { "address", "port" } ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Hardware::State::FAILED );
				return;
			}

			if ( this->m_connection != nullptr ) {
				this->m_connection->terminate();
			}

			std::string uri = this->m_settings->get( "address" ) + ':' + this->m_settings->get( "port" );
			this->m_connectionState = ConnectionState::IDLE;
			this->m_connection = Network::connect( uri, [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) {
				switch( event_ ) {
					case Network::Connection::Event::CONNECT: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );

						// Once we're connected the currently active activity is requested.
						std::stringstream response;
						response << "<stream:stream to='connect.logitech.com' xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:client' xml:lang='en' version='1.0'>";
						response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?getCurrentActivity\"></oa></iq>";
						connection_->send( response.str() );
						
						this->m_connectionState = ConnectionState::WAIT_FOR_CURRENT_ACTIVITY;
						break;
					}
					case Network::Connection::Event::FAILURE: {
						Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
						this->setState( Hardware::State::FAILED );
						this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, this->m_task );
						break;
					}
					case Network::Connection::Event::DATA: {
						const std::string& data = connection_->popData();
						this->m_received.append( data );
						if (
							! data.empty()
							&& data.at( data.size() - 1 ) == '>'
						) {
							this->_process();
							this->m_received.clear();
						}
						break;
					}
					case Network::Connection::Event::DROPPED: {
						Logger::log( Logger::LogLevel::WARNING, this, "Connection dropped." );
						this->setState( Hardware::State::FAILED );
						this->m_scheduler.schedule( SCHEDULER_INTERVAL_1MIN, 1, this->m_task );
						break;
					}
					case Network::Connection::Event::CLOSE: {
						Logger::log( Logger::LogLevel::VERBOSE, this, "Connection closed." );
						break;
					}
					default: { break; }
				}
			} );
		} );

		this->m_scheduler.schedule( 1000 * HARMONY_HUB_PING_INTERVAL_SEC, SCHEDULER_INFINITE, this, "harmony hub pinger", [this]( Scheduler::Task<>& task_ ) {
			if ( this->getState() == Hardware::State::READY ) {
				std::stringstream response;
				response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID;
				response << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.connect/vnd.logitech.ping\"></oa></iq>";
				this->m_connection->send( response.str() );
			}
		} );
	};
	
	void HarmonyHub::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		if ( this->m_connection != nullptr ) {
			this->m_connection->terminate();
		}
		Hardware::stop();
	};

	json HarmonyHub::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["address"] = this->m_settings->get( "address", "" );
		result["port"] = this->m_settings->get( "port", "" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json HarmonyHub::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		Hardware::State state = this->getState();
		result += {
			{ "name", "address" },
			{ "label", "Address" },
			{ "type", "string" },
			{ "class", state >= Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 98 }
		};
		result += {
			{ "name", "port" },
			{ "label", "Port" },
			{ "type", "short" },
			{ "min", "1" },
			{ "max", "65536" },
			{ "class", state >= Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};

	bool HarmonyHub::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		apply_ = false;

		if ( this->getState() != Hardware::State::READY ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Harmony Hub not ready (yet)." );
			return false;
		}

		std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
		
		std::string startActivityId = "";
		if (
			device->getValueOption() == Switch::Option::OFF
			&& device->getReference() == this->m_currentActivityId
		) {
			if ( device->getReference() == "-1" ) {
				startActivityId = this->m_settings->get( HARMONY_HUB_LAST_ACTIVITY_SETTING, "-1" );
				if ( startActivityId == "-1" ) {
					Logger::log( Logger::LogLevel::WARNING, this, "Unable to start last known activity." );
				} else {
					Logger::logr( Logger::LogLevel::VERBOSE, this, "Starting last known activity." );
				}
			} else {
				startActivityId = "-1"; // PowerOff
			}
		}
		if (
			device->getValueOption() == Switch::Option::ON
			&& device->getReference() != this->m_currentActivityId
		) {
			startActivityId = device_->getReference();
		}
		
		if ( startActivityId != "" ) {
			if ( this->_queuePendingUpdate( "harmony_hub_" + std::to_string( this->m_id ), source_, HARMONY_HUB_BUSY_BLOCK_MSEC, HARMONY_HUB_BUSY_WAIT_MSEC ) ) {
				std::stringstream command;
				command << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID;
				command << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?startactivity\">activityId=";
				command << startActivityId << ":timestamp=0</oa></iq>";
				this->m_connection->send( command.str() );
				return true;
			} else {
				Logger::log( Logger::LogLevel::ERROR, this, "Harmony Hub busy." );
				return false;
			}
		} else {
			Logger::log( Logger::LogLevel::ERROR, this, "Invalid activity." );
			return false;
		}

		return true;
	};

	void HarmonyHub::_process() {
		if ( this->m_received == "<iq/>" ) {
			return;
		}

		switch( this->m_connectionState ) {
			case ConnectionState::WAIT_FOR_CURRENT_ACTIVITY: {
				if (
					this->m_received.find( "vnd.logitech.harmony.engine?getCurrentActivity" ) != std::string::npos
					&& stringIsolate( this->m_received, "<![CDATA[result=", "]]>", this->m_currentActivityId )
				) {
					std::stringstream response;
					response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?config\"></oa></iq>";
					this->m_connection->send( response.str() );
					this->m_connectionState = ConnectionState::WAIT_FOR_ACTIVITIES;
				}
				break;
			}
			case ConnectionState::WAIT_FOR_ACTIVITIES: {
				std::string raw;
				if (
					this->m_received.find( "vnd.logitech.harmony/vnd.logitech.harmony.engine?config" ) != std::string::npos
					&& stringIsolate( this->m_received, "<![CDATA[", "]]>", raw )
				) {
					try {
						json data = json::parse( raw );
						json activities = jsonGet<json>( data, "activity" );
						for ( auto activityIt = activities.begin(); activityIt != activities.end(); activityIt++ ) {
							json activity = *activityIt;
							std::string activityId = jsonGet<std::string>( activity, "id" );
							std::string label = jsonGet<std::string>( activity, "label" );
							auto device = this->declareDevice<Switch>( activityId, label, {
								{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
								{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::GENERIC ) }
							} );
							if ( activityId == this->m_currentActivityId ) {
								device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::ON );
							} else {
								device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::OFF );
							}
						}
						this->m_connectionState = ConnectionState::IDLE;
						this->setState( Hardware::State::READY );
					} catch( json::exception ex_ ) {
						Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
						this->m_connection->terminate(); // results in dropped vs. closed
						break;
					}
				}
			}
			case ConnectionState::IDLE: {
				if (
					this->m_received.find( "vnd.logitech.ping" ) != std::string::npos
					&& this->m_received.find( "errorcode='200'" ) == std::string::npos
				) {
					Logger::log( Logger::LogLevel::ERROR, this, "Invalid ping response." );
					this->m_connection->terminate(); // results in dropped vs. closed
					break;
				}

				std::string raw;
				if (
					this->m_received.find( "connect.stateDigest?notify" ) != std::string::npos
					&& stringIsolate( this->m_received, "<![CDATA[", "]]>", raw )
				) {
					try {
						json data = json::parse( raw );
						int activityStatus = jsonGet<int>( data, "activityStatus" );
						std::string activityId = jsonGet<std::string>( data, "activityId" );

						// ActivityStatus 3 if reported just before a full poweroff on the activity that is being
						// turned off. We're masquarading this as a full power off.
						if ( 3 == activityStatus ) {
							activityStatus = 0;
							activityId = "-1";
						}

						if (
							1 == activityStatus
							|| 0 == activityStatus // full power off
						) {
							Device::UpdateSource source = Device::UpdateSource::HARDWARE;
							this->_releasePendingUpdate( "harmony_hub_" + std::to_string( this->m_id ), source );

							// First switch off the current activity if it differes from the one being started.
							if ( this->m_currentActivityId != activityId ) {
								std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->getDevice( this->m_currentActivityId ) );
								if (
									device != nullptr
									&& device->getValueOption() != Switch::Option::OFF
								) {
									device->updateValue( source, Switch::Option::OFF );
								}
							}

							// Then turn on the activity if it's not already on.
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->getDevice( activityId ) );
							if (
								device != nullptr
								&& device->getValueOption() != Switch::Option::ON
							) {
								device->updateValue( source, Switch::Option::ON );

								// Store the last non-PowerOff activity in settings. This is used when the PowerOff
								// activity is turned off directly without a specific activity.
								if ( activityId != "-1" ) {
									this->m_settings->put( HARMONY_HUB_LAST_ACTIVITY_SETTING, activityId );
								}
							}

							this->m_currentActivityId = activityId;
						}
					} catch( json::exception ex_ ) {
						Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
						this->m_connection->terminate(); // results in dropped vs. closed
						break;
					}
				}

#ifdef _DEBUG
				std::string activityId;
				if (
					this->m_received.find( "startActivityFinished" ) != std::string::npos
					&& stringIsolate( this->m_received, "activityId=", ":", activityId )
				) {
					assert( this->m_currentActivityId == activityId && "Activity finished notification should match currecnt active activity." );
				}
#endif // _DEBUG
				break;
			}
		}
	};
	
}; // namespace micasa
