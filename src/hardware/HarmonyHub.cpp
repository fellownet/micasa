#include "HarmonyHub.h"

#include "json.hpp"

#include "../Utils.h"
#include "../device/Switch.h"
#include "../Network.h"

#define HARMONY_HUB_CONNECTION_ID		"21345678-1234-5678-1234-123456789012-1"
#define HARMONY_HUB_PING_INTERVAL_SEC	30

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Network> g_network;

	using namespace nlohmann;
	
	void HarmonyHub::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();
	}
	
	void HarmonyHub::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	}

	const std::chrono::milliseconds HarmonyHub::_work( const unsigned long int& iteration_ ) {
		if ( ! this->m_settings.contains( { "address", "port" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			return std::chrono::milliseconds( 1000 * 60 * 5 );
		}

		if ( this->m_state == CLOSED ) {
			std::string uri = this->m_settings["address"] + ':' + this->m_settings["port"];
			this->m_state = CONNECTING;
			this->m_connection = g_network->connect( uri, Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
				switch( event_ ) {
					case MG_EV_CONNECT: {
						int status = *(int*) data_;
						if ( status == 0 ) {
							mg_set_timer( connection_, 0 ); // clear timeout timer
							this->_processConnection( true );
						} else {
							this->_processConnection( false );
						}
						mg_set_timer( this->m_connection, mg_time() + HARMONY_HUB_PING_INTERVAL_SEC );
						break;
					}
					case MG_EV_RECV:
						this->_processConnection( true );
						break;
						
					case MG_EV_POLL:
					case MG_EV_SEND:
					case MG_EV_SHUTDOWN:
						break;
						
					case MG_EV_TIMER: {
#ifdef _DEBUG
						g_logger->log( Logger::LogLevel::DEBUG, this, "Sending ping." );
#endif // _DEBUG
						std::stringstream send;
						send << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID;
						send << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.connect/vnd.logitech.ping\"></oa></iq>";
						mg_send( this->m_connection, send.str().c_str(), send.str().size() );
						mg_set_timer( this->m_connection, mg_time() + HARMONY_HUB_PING_INTERVAL_SEC );
						break;
					}
						
					default:
						this->_processConnection( false );
						break;
				}
			} ) );
		}
		
		return std::chrono::milliseconds( 1000 * 60 * 5 );
	}
	
	bool HarmonyHub::updateDevice( const unsigned int& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		apply_ = false;

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
			} else {
				g_logger->log( Logger::LogLevel::ERROR, this, "Harmony Hub busy." );
				return false;
			}
		}

		return true;
	}
	
	void HarmonyHub::_disconnect( const std::string message_ ) {
		g_logger->log( Logger::LogLevel::ERROR, this, message_ );
		this->m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
		this->m_state = CLOSED;
	}
	
	void HarmonyHub::_processConnection( const bool ready_ ) {
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

			switch( this->m_state ) {
				case CLOSED: {
					break;
				}
					
				case CONNECTING: {
					// Upon connecting we're immediately requesting the currently active activity.
					std::stringstream response;
					response << "<stream:stream to='connect.logitech.com' xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:client' xml:lang='en' version='1.0'>";
					response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?getCurrentActivity\"></oa></iq>";
					mg_send( this->m_connection, response.str().c_str(), response.str().size() );
					this->m_state = WAIT_FOR_CURRENT_ACTIVITY;
					break;
				}

				case WAIT_FOR_CURRENT_ACTIVITY: {
					if (
						received.find( "vnd.logitech.harmony.engine?getCurrentActivity" ) != std::string::npos
						&& stringIsolate( received, "<![CDATA[result=", "]]>", this->m_currentActivityId )
					) {
						// When the currently active activity was received we're requesting the configuration of the
						// hub, which includes the list of available activities.
						std::stringstream response;
						response << "<iq type=\"get\" id=\"" << HARMONY_HUB_CONNECTION_ID << "\"><oa xmlns=\"connect.logitech.com\" mime=\"vnd.logitech.harmony/vnd.logitech.harmony.engine?config\"></oa></iq>";
						mg_send( this->m_connection, response.str().c_str(), response.str().size() );
						this->m_state = WAIT_FOR_ACTIVITIES;
						
					}
					break;
				}
					
				case WAIT_FOR_ACTIVITIES: {
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
						
						if ( data["activity"].is_array() ) {
							for ( auto activityIt = data["activity"].begin(); activityIt != data["activity"].end(); activityIt++ ) {
								json activity = *activityIt;
								if (
									! activity["id"].is_null()
									&& ! activity["label"].is_null()
								) {
									std::string activityId = activity["id"].get<std::string>();
									std::string label = activity["label"].get<std::string>();
									if ( activityId != "-1" ) {
										std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->_declareDevice( Device::Type::SWITCH, activityId, label, {
											{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, std::to_string( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE | Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) }
										} ) );
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
							this->m_state = IDLE;
						} else {
							this->_disconnect( "Malformed activities data." );
							break;
						}
					}
				}
					
				case IDLE: {
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
							data.is_object()
							&& ! data["activityStatus"].is_null()
						) {
							int activityStatus = data["activityStatus"].get<int>();
							if ( 1 == activityStatus ) {
								// When the hardware initiates a switch the hub should also be locked from new updates
								// while the update is pending.
								this->_queuePendingUpdate( "hub", Device::UpdateSource::HARDWARE, 0, HARMONY_HUB_BUSY_WAIT_MSEC );
							}
						}
					}
					
					// Detect "startActivityFinished" events which indicate a finalized activity change.
					std::string activityId;
					if (
						received.find( "startActivityFinished" ) != std::string::npos
						&& stringIsolate( received, "activityId=", ":", activityId )
					) {
						// Release any pending updates and use the corresponding update source for our source.
						unsigned int source = Device::UpdateSource::HARDWARE;
						source |= this->_releasePendingUpdate( "hub" );

						// Turn off the current activity when a different activity was started than the one currently
						// active, or on full power off (=-1).
						if (
							activityId == "-1" // full power off
							|| (
								this->m_currentActivityId != "-1"
								&& this->m_currentActivityId != activityId
							)
						) {
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->_getDevice( this->m_currentActivityId ) );
							if ( device != nullptr ) {
								device->updateValue( source, Switch::Option::OFF );
							}
							this->m_currentActivityId = "-1";
						}
						
						// Turn the new activity on.
						if ( activityId != "-1" ) {
							std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( this->_getDevice( activityId ) );
							if ( device != nullptr ) {
								device->updateValue( source, Switch::Option::ON );
							}
							this->m_currentActivityId = activityId;
						}
					}
					break;
				}
			}
			
			received.clear();
			
		} else if ( this->m_state != CLOSED ) {
			this->_disconnect( "Connection closed." );
		}
	}
	
}; // namespace micasa
