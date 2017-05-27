// https://core.telegram.org/bots/api#making-requests

#include <sstream>

#include "../Logger.h"
#include "../Network.h"
#include "../User.h"
#include "../device/Switch.h"
#include "../device/Text.h"

#include "Telegram.h"

namespace micasa {

	using namespace nlohmann;

	const char* Telegram::label = "Telegram Bot";

	Telegram::Telegram( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) :
		Hardware( id_, type_, reference_, parent_ ),
		m_username( "" ),
		m_lastUpdateId( -1 ),
		m_acceptMode( false )
	{
	};

	void Telegram::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		// Add the devices that will initiate controller actions and the broadcast device.
		this->declareDevice<Switch>( "accept", "Enable Accept Mode", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::HARDWARE | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::DISABLED );
		this->declareDevice<Text>( "broadcast", "Broadcast", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveTextSubType( Text::SubType::NOTIFICATION ) }
		} );

		this->m_username = "";
		this->m_task = this->m_scheduler.schedule( 0, 1, this, [this]( std::shared_ptr<Scheduler::Task<>> ) {
			if ( ! this->m_settings->contains( { "token" } ) ) {
				Logger::log( Logger::LogLevel::ERROR, this, "Missing settings." );
				this->setState( Hardware::State::FAILED );
				return;
			}

			if ( this->m_connection != nullptr ) {
				this->m_connection->terminate();
			}

			std::stringstream url;
			url << "https://api.telegram.org/bot" << this->m_settings->get( "token" );
			if ( this->m_username.empty() ) {

				url << "/getMe";
				this->m_connection = Network::connect( url.str(), [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) {
					switch( event_ ) {
						case Network::Connection::Event::CONNECT: {
							Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );
							break;
						}
						case Network::Connection::Event::FAILURE: {
							Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
							this->setState( Hardware::State::FAILED );
							this->m_scheduler.schedule( SCHEDULER_INTERVAL_5SEC, 1, this->m_task );
							break;
						}
						case Network::Connection::Event::HTTP: {
							if ( ! this->_process( connection_->getBody() ) ) {
								this->setState( Hardware::State::FAILED );
							}
							break;
						}
						case Network::Connection::Event::CLOSE: {
							Logger::log( Logger::LogLevel::VERBOSE, this, "Connection closed." );
							if ( this->getState() != Hardware::State::FAILED ) {
								this->setState( Hardware::State::DISCONNECTED );
								this->m_scheduler.schedule( SCHEDULER_INTERVAL_1SEC, 1, this->m_task );
							} else {
								this->m_scheduler.schedule( SCHEDULER_INTERVAL_5SEC, 1, this->m_task );
							}
							break;
						}
						default: { break; }
					}
				} );

			} else {

				url << "/getUpdates";
				json params = {
					{ "timeout", 60 }
				};
				if ( this->m_lastUpdateId > -1 ) {
					params["offset"] = this->m_lastUpdateId + 1;
				}
				this->m_connection = Network::connect( url.str(), params, [this]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) {
					switch( event_ ) {
						case Network::Connection::Event::CONNECT: {
							Logger::log( Logger::LogLevel::VERBOSE, this, "Connected." );
							this->setState( Hardware::State::READY );
							break;
						}
						case Network::Connection::Event::FAILURE: {
							Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
							this->setState( Hardware::State::FAILED );
							this->m_scheduler.schedule( SCHEDULER_INTERVAL_5SEC, 1, this->m_task );
							break;
						}
						case Network::Connection::Event::HTTP: {
							if ( ! this->_process( connection_->getBody() ) ) {
								this->setState( Hardware::State::FAILED );
							}
							break;
						}
						case Network::Connection::Event::CLOSE: {
							Logger::log( Logger::LogLevel::VERBOSE, this, "Connection closed." );
							if ( this->getState() != Hardware::State::FAILED ) {
								this->setState( Hardware::State::DISCONNECTED );
								this->m_scheduler.schedule( SCHEDULER_INTERVAL_1SEC, 1, this->m_task );
							} else {
								this->m_scheduler.schedule( SCHEDULER_INTERVAL_5SEC, 1, this->m_task );
							}
							break;
						}
						default: { break; }
					}
				} );
			}
		} );

	};
	
	void Telegram::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		this->m_scheduler.erase( [this]( const Scheduler::BaseTask& task_ ) {
			return task_.data == this;
		} );
		if ( this->m_connection != nullptr ) {
			this->m_connection->terminate();
		}
		Hardware::stop();
	};

	std::string Telegram::getLabel() const {
		if ( this->m_username.size() ) {
			std::stringstream label;
			label << Telegram::label << " (" << this->m_username << ")";
			return label.str();
		} else {
			return Telegram::label;
		}
	};

	bool Telegram::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		if ( this->getState() < Hardware::State::DISCONNECTED ) {
			Logger::log( Logger::LogLevel::ERROR, this, "Telegram not ready (yet)." );
			return false;
		}

		if ( device_->getType() == Device::Type::SWITCH ) {
			std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
			if (
				device->getReference() == "accept"
				&& device->getValueOption() == Switch::Option::ENABLED
			) {
				if ( this->m_acceptMode ) {
					Logger::log( Logger::LogLevel::ERROR, this, "Accept Mode already enabled." );
				} else {
					Logger::log( Logger::LogLevel::NORMAL, this, "Accept Mode enabled." );
					this->m_acceptMode = true;
					this->m_scheduler.schedule( SCHEDULER_INTERVAL_5MIN, 1, this, [this,device]( std::shared_ptr<Scheduler::Task<>> ) {
						this->m_acceptMode = false;
						device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::DISABLED );
						Logger::log( Logger::LogLevel::VERBOSE, this, "Accept Mode disabled." );
					} );
				}
				return true;
			}
		} else if ( device_->getType() == Device::Type::TEXT ) {

			// A message can be sent to a single device (=chat) or to the broadcast device, in which case it is send
			// to all chat devices.
			auto sourceDevice = std::static_pointer_cast<Text>( device_ );
			std::vector<std::shared_ptr<Text>> targetDevices;
			if ( sourceDevice->getReference() == "broadcast" ) {
				auto allDevices = this->getAllDevices();
				for ( auto deviceIt = allDevices.begin(); deviceIt != allDevices.end(); deviceIt++ ) {
					if (
						(*deviceIt)->getType() == Device::Type::TEXT
						&& (*deviceIt)->getReference() != "broadcast"
						&& (*deviceIt)->isEnabled()
					) {
						targetDevices.push_back( std::static_pointer_cast<Text>( *deviceIt ) );
					}
				}
			} else {
				targetDevices.push_back( sourceDevice );
				apply_ = false;
			}
			for ( auto deviceIt = targetDevices.begin(); deviceIt != targetDevices.end(); deviceIt++ ) {

				// For each device a task is started that makes sure several attempts are made to send the message. The
				// target device is captured as a weak pointer to make sure the device can be destroyed if necessary.
				std::string value = sourceDevice->getValue();
				Device::UpdateSource source = source_;
				std::string reference = (*deviceIt)->getReference();
				std::weak_ptr<Text> targetPtr = *deviceIt;

				// The mutable flag on the lambda ensures that the variables are copied and mutable. Otherwise they're
				// captured as a const by default and we can't modify them (NOTE they're still copies!).
				if ( this->_queuePendingUpdate( reference, source_, TELEGRAM_BUSY_BLOCK_MSEC, TELEGRAM_BUSY_WAIT_MSEC ) ) {
					this->m_scheduler.schedule( 0, 1, this, [this,value,targetPtr,source,reference]( std::shared_ptr<Scheduler::Task<>> task_ ) mutable {
						auto target = targetPtr.lock();
						if ( target ) {
							std::stringstream url;
							url << "https://api.telegram.org/bot" << this->m_settings->get( "token" ) << "/sendMessage";
							json params = {
								{ "chat_id", std::stoi( target->getReference() ) },
								{ "text", value },
								{ "parse_mode", "Markdown" }
							};
							Network::connect( url.str(), params, [this,value,targetPtr,source,reference,task_]( std::shared_ptr<Network::Connection> connection_, Network::Connection::Event event_ ) mutable {
								switch( event_ ) {
									case Network::Connection::Event::CONNECT: {
										this->_releasePendingUpdate( reference, source );
										auto target = targetPtr.lock();
										if ( target ) {
											target->updateValue( source | Device::UpdateSource::HARDWARE, value );
										}
										break;
									}
									case Network::Connection::Event::FAILURE: {
										if ( task_->iteration < 5 ) {
											Logger::log( Logger::LogLevel::WARNING, this, "Connection failure, retrying." );
											this->m_scheduler.schedule( SCHEDULER_INTERVAL_1SEC, 1, task_ );
										} else {
											Logger::log( Logger::LogLevel::ERROR, this, "Connection failure." );
											this->_releasePendingUpdate( reference );
										}
										break;
									}
									default: { break; }
								}
							} );
						}
					} );
				}
			}

			return true;
		}

		return false;
	};

	json Telegram::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["token"] = this->m_settings->get( "token", "" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json Telegram::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		result += {
			{ "name", "token" },
			{ "label", "Token" },
			{ "type", "string" },
			{ "class", this->getState() >= Hardware::State::READY ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};

	bool Telegram::_process( const std::string& data_ ) {
		try {
			json data = json::parse( data_ );
			bool success = jsonGet<bool>( data, "ok" );
			if ( ! success ) {
				this->setState( Hardware::State::FAILED );
				Logger::log( Logger::LogLevel::ERROR, this, jsonGet<std::string>( data, "description" ) + "." );
				return false;
			}

			json result = jsonGet<json>( data, "result" );
			if ( result.is_object() ) {
				auto find = result.find( "username" );
				if ( find != result.end() ) {
					this->m_username = jsonGet<std::string>( *find );
				}
			}

			if ( result.is_array() ) {
				for ( auto updatesIt = result.begin(); updatesIt != result.end(); updatesIt++ ) {

					int lastUpdateId = jsonGet<int>( *updatesIt, "update_id" );
					if ( lastUpdateId > this->m_lastUpdateId ) {
						this->m_lastUpdateId = lastUpdateId;
					}

					json message = jsonGet<json>( *updatesIt, "message" );
					json chat = jsonGet<json>( message, "chat" );
					std::string reference = jsonGet<std::string>( chat, "id" );

					auto device = std::static_pointer_cast<Text>( this->getDevice( reference ) );
					if (
						device == nullptr
						&& this->m_acceptMode
					) {
						std::stringstream label;
						if ( chat.find( "first_name" ) != chat.end() ) {
							label << jsonGet<std::string>( chat, "first_name" );
						}
						if ( chat.find( "last_name" ) != chat.end() ) {
							if ( label.str().size() > 0 ) {
								label << " ";
							}
							label << jsonGet<std::string>( chat, "last_name" );
						}
						if ( chat.find( "username" ) != chat.end() ) {
							if ( label.str().size() > 0 ) {
								label << " (" << jsonGet<std::string>( chat, "username" ) << ")";
							} else {
								label << jsonGet<std::string>( chat, "username" );
							}
						}
						if ( label.str().size() == 0 ) {
							label << reference;
						}
						device = this->declareDevice<Text>( reference, label.str(), {
							{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
							{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveTextSubType( Text::SubType::NOTIFICATION ) }
						} );
					}
					if ( device != nullptr ) {
						device->updateValue( Device::UpdateSource::HARDWARE, jsonGet<std::string>( message, "text" ) );
					}
				}
			}

			return true;
		} catch( json::exception ex_ ) {
			this->setState( Hardware::State::FAILED );
			Logger::log( Logger::LogLevel::ERROR, this, ex_.what() );
			return false;
		}
	};

}; // namespace micasa
