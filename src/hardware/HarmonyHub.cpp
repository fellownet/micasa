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

	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;
	
	void HarmonyHub::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
		
		// Connect to the configured Harmony Hub. This method will set the state of the hardware.
		this->_connect();
	};
	
	void HarmonyHub::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		if ( this->m_connectionState != ConnectionState::CLOSED ) {
			this->m_connectionState = ConnectionState::CLOSED;
			this->m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
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
		result += {
			{ "name", "address" },
			{ "label", "Address" },
			{ "type", "string" },
			{ "class", this->m_settings->contains( "address" ) ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 98 }
		};
		result += {
			{ "name", "port" },
			{ "label", "Port" },
			{ "type", "short" },
			{ "min", "1" },
			{ "max", "65536" },
			{ "class", this->m_settings->contains( "port" ) ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};

	bool HarmonyHub::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		apply_ = false;

		if ( this->getState() != Hardware::State::READY ) {
			return false;
		}

		std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
		
		std::string startActivityId = "";
		if (
			device->getValueOption() == Switch::Option::OFF
			&& device->getReference() == this->m_currentActivityId
		) {
			startActivityId = "-1"; // PowerOff
		}
		if (
			device->getValueOption() == Switch::Option::ON
			&& device->getReference() != this->m_currentActivityId
		) {
			startActivityId = device_->getReference();
		}
		
		if ( startActivityId != "" ) {
			if ( this->_queuePendingUpdate( "hub", source_, HARMONY_HUB_BUSY_BLOCK_MSEC, HARMONY_HUB_BUSY_WAIT_MSEC ) ) {
				if ( device->getValueOption() == Switch::Option::ON ) {
					g_logger->logr( Logger::LogLevel::NORMAL, this, "Starting activity %s.", device_->getLabel().c_str() );
				} else {
					g_logger->logr( Logger::LogLevel::NORMAL, this, "Stopping activity %s.", device_->getLabel().c_str() );
				}
				
				std::stringstream response;
				response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID;
				response << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?startactivity\">activityId=";
				response << startActivityId << ":timestamp=0</oa></iq>";
				mg_send( this->m_connection, response.str().c_str(), response.str().size() );
				return true;
			} else {
				g_logger->log( Logger::LogLevel::ERROR, this, "Harmony Hub busy." );
				return false;
			}
		} else {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid activity." );
			return false;
		}
	};
	
	void HarmonyHub::_connect() {
#ifdef _DEBUG
		assert( this->m_connectionState == ConnectionState::CLOSED && "HarmonyHub should not already be connected when connecting." );
#endif // _DEBUG
		if ( ! this->m_settings->contains( { "address", "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return;
		}

		std::string uri = this->m_settings->get( "address" ) + ':' + this->m_settings->get( "port" );
		this->m_connectionState = ConnectionState::CONNECTING;
		
		// A weak pointer to this is captured into the connection handler to make sure the handler doesn't keep the
		// hardware from being destroyed by the controller.
		std::weak_ptr<HarmonyHub> ptr = std::static_pointer_cast<HarmonyHub>( this->shared_from_this() );
		this->m_connection = Network::get().connect( uri, Network::t_callback( [ptr]( mg_connection* connection_, int event_, void* data_ ) {
			auto me = ptr.lock();
			if ( me ) {
				switch( event_ ) {
					case MG_EV_CONNECT: {
						int status = *(int*) data_;
						if ( status == 0 ) {
							mg_set_timer( connection_, 0 ); // clear timeout timer
							me->_process( true );
						} else {
							me->_process( false );
						}
						mg_set_timer( me->m_connection, mg_time() + HARMONY_HUB_PING_INTERVAL_SEC );
						break;
					}
					case MG_EV_RECV:
						me->_process( true );
						break;
						
					case MG_EV_POLL:
					case MG_EV_SEND:
						break;
						
					case MG_EV_TIMER: {
#ifdef _DEBUG
						g_logger->log( Logger::LogLevel::DEBUG, me, "Sending ping." );
#endif // _DEBUG
						std::stringstream send;
						send << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID;
						send << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.connect/vnd.logitech.ping\"></oa></iq>";
						mg_send( me->m_connection, send.str().c_str(), send.str().size() );
						mg_set_timer( me->m_connection, mg_time() + HARMONY_HUB_PING_INTERVAL_SEC );
						break;
					}
						
					default:
						me->_process( false );
						break;
				}
			} else {
				connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
			}
		} ) );
	};

	void HarmonyHub::_disconnect( const std::string message_ ) {
		g_logger->log( Logger::LogLevel::ERROR, this, message_ );
		this->m_connectionState = ConnectionState::CLOSED;
		this->m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
		this->setState( Hardware::State::FAILED );

		// Try to reconnect automatically in 1 minute if the connection was unexpectedly dropped.
		this->m_scheduler.schedule( SCHEDULER_INTERVAL_1MIN, 1, NULL, [this]( Scheduler::Task<>& ) {
			this->_connect();
		} );
	};
	
	void HarmonyHub::_process( const bool ready_ ) {
		if ( ready_ ) {
			
			static std::string received;

			// Tje received string is filled with the buffer and preprocessed. If incomplete- or unimportant data was
			// received it will not (yet) be processed.
			struct mbuf *rcvBuffer = &this->m_connection->recv_mbuf;
			received.append( rcvBuffer->buf, rcvBuffer->len );
			mbuf_remove( rcvBuffer, rcvBuffer->len );
			
			if (
				received.size() > 0
				&& received.at( received.size() - 1 ) != '>'
			) {
				return;
			}
			if ( received == "<iq/>" ) {
				return;
			}

			switch( this->m_connectionState ) {
				case ConnectionState::CLOSED: {
					break;
				}
					
				case ConnectionState::CONNECTING: {
					// Upon connecting we're immediately requesting the currently active activity.
					std::stringstream response;
					response << "<stream:stream to='connect.logitech.com' xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:client' xml:lang='en' version='1.0'>";
					response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?getCurrentActivity\"></oa></iq>";
					mg_send( this->m_connection, response.str().c_str(), response.str().size() );
					this->m_connectionState = ConnectionState::WAIT_FOR_CURRENT_ACTIVITY;
					break;
				}

				case ConnectionState::WAIT_FOR_CURRENT_ACTIVITY: {
					if (
						received.find( "vnd.logitech.harmony.engine?getCurrentActivity" ) != std::string::npos
						&& stringIsolate( received, "<![CDATA[result=", "]]>", this->m_currentActivityId )
					) {
						// When the currently active activity was received we're requesting the configuration of the
						// hub, which includes the list of available activities.
						std::stringstream response;
						response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?config\"></oa></iq>";
						mg_send( this->m_connection, response.str().c_str(), response.str().size() );
						this->m_connectionState = ConnectionState::WAIT_FOR_ACTIVITIES;
						
					}
					break;
				}
					
				case ConnectionState::WAIT_FOR_ACTIVITIES: {
					std::string raw;
					if (
						received.find( "vnd.logitech.harmony/vnd.logitech.harmony.engine?config" ) != std::string::npos
						&& stringIsolate( received, "<![CDATA[", "]]>", raw )
					) {
						json data;
						try {
							data = json::parse( raw );
						} catch( std::invalid_argument ) {
							this->_disconnect( "Malformed activities data." );
							break;
						}
						
						if (
							data.find( "activity" ) != data.end()
							&& data["activity"].is_array()
						) {
							for ( auto activityIt = data["activity"].begin(); activityIt != data["activity"].end(); activityIt++ ) {
								json activity = *activityIt;
								if (
									! activity["id"].is_null()
									&& ! activity["label"].is_null()
								) {
									std::string activityId = activity["id"].get<std::string>();
									std::string label = activity["label"].get<std::string>();
									if ( activityId != "-1" ) {
										auto device = this->declareDevice<Switch>( activityId, label, {
											{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
											{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveSubType( Switch::SubType::GENERIC ) }
										} );
										if ( activityId == this->m_currentActivityId ) {
											device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::ON );
										} else {
											device->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::OFF );
										}
									}
								}
							}
							
							// After the activities have been recieved we're putting the connection in idle state. At this
							// point changes in activities can be received and processed.
							this->m_connectionState = ConnectionState::IDLE;
							this->setState( Hardware::State::READY );
						} else {
							this->_disconnect( "Malformed activities data." );
							break;
						}
					}
				}
					
				case ConnectionState::IDLE: {
					// Detect and analyze ping responses.
					if (
						received.find( "vnd.logitech.ping" ) != std::string::npos
						&& received.find( "errorcode='200'" ) == std::string::npos
					) {
						this->_disconnect( "Invalid ping response." );
						break;
					}
					
					// When the first notification of an activity change comes int we're going to lock the hub to prevent
					// other commands from being sent during the change.
					std::string raw;
					if (
						received.find( "connect.stateDigest?notify" ) != std::string::npos
						&& stringIsolate( received, "<![CDATA[", "]]>", raw )
					) {
						json data;
						try {
							data = json::parse( raw );
						} catch( std::invalid_argument ) {
							this->_disconnect( "Malformed notification data." );
							break;
						}
						
						if (
							data.find( "activityStatus" ) != data.end()
							&& data["activityStatus"].is_number()
						) {
							int activityStatus = data["activityStatus"].get<int>();
							if ( 1 == activityStatus ) {
								// When the hardware initiates a switch the hub should also be locked from new updates
								// while the update is pending.
								this->_queuePendingUpdate( "hub", Device::UpdateSource::HARDWARE, 0, HARMONY_HUB_BUSY_WAIT_MSEC );
							}
						} else {
							this->_disconnect( "Malformed activity status data." );
							break;
						}
					}
					
					// Detect "startActivityFinished" events which indicate a finalized activity change.
					std::string activityId;
					if (
						received.find( "startActivityFinished" ) != std::string::npos
						&& stringIsolate( received, "activityId=", ":", activityId )
					) {
						// Release any pending updates and use the corresponding update source for our source.
						Device::UpdateSource source = Device::UpdateSource::HARDWARE;
						this->_releasePendingUpdate( "hub", source );

						// Turn off the current activity when a different activity was started than the one currently
						// active, or on full power off (=-1).
						if (
							activityId == "-1" // full power off
							|| (
								this->m_currentActivityId != "-1"
								&& this->m_currentActivityId != activityId
							)
						) {
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->getDevice( this->m_currentActivityId ) );
							if ( device != nullptr ) {
								device->updateValue( source, Switch::Option::OFF );
							}
							this->m_currentActivityId = "-1";
						}
						
						// Turn the new activity on.
						if ( activityId != "-1" ) {
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->getDevice( activityId ) );
							if ( device != nullptr ) {
								device->updateValue( source, Switch::Option::ON );
								this->m_currentActivityId = activityId;
							}
						}
					}
					break;
				}
			}
			
			received.clear();
			
		// If the state is already set to CLOSED the cause has already been taken care of. This can be a hardware
		// shutdown or the _disconnect method has already ben called.
		} else if ( this->m_connectionState != ConnectionState::CLOSED ) {
			this->_disconnect( "Connection closed." );
		}
	};
	
}; // namespace micasa
