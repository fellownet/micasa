// https://core.telegram.org/bots/api#making-requests

#include "../Logger.h"
#include "../Network.h"
#include "../User.h"
#include "../device/Switch.h"
#include "../device/Text.h"

#include "Telegram.h"

namespace micasa {

	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;

	void Telegram::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		// Add the devices that will initiate controller actions and the broadcast device.
		this->declareDevice<Switch>( "accept", "Enable Accept Mode", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::INIT | Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
		this->declareDevice<Text>( "broadcast", "Broadcast", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveSubType( Text::SubType::NOTIFICATION ) }
		} );
	};
	
	void Telegram::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	};

	std::string Telegram::getLabel() const throw() {
		if ( this->m_username.size() ) {
			std::stringstream label;
			label << Telegram::label << " (" << this->m_username << ")";
			return label.str();
		} else {
			return Telegram::label;
		}
	};

	std::chrono::milliseconds Telegram::_work( const unsigned long int& iteration_ ) {
		if ( ! this->m_settings->contains( { "token" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		std::stringstream url;
		url << "https://api.telegram.org/bot" << this->m_settings->get( "token" );

		if (
			iteration_ == 1
			|| this->getState() != Hardware::State::READY
		) {
			url << "/getMe";

			Network::get().connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
				if ( event_ == MG_EV_HTTP_REPLY ) {
					std::string body;
					body.assign( ((http_message*)data_)->body.p, ((http_message*)data_)->body.len );
					try {
						json data = json::parse( body );
						bool success = data["ok"].get<bool>();
						if ( ! success ) {
							throw std::runtime_error( "Telegram API reported a failure" );
						}

						this->m_username = data["result"]["username"].get<std::string>();
						this->setState( Hardware::State::READY );

					} catch( std::invalid_argument ex_ ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Invalid data received from the Telegram API." );
						this->setState( Hardware::State::FAILED );
					} catch( std::runtime_error ex_ ) {
						g_logger->log( Logger::LogLevel::ERROR, this, ex_.what() );
						this->setState( Hardware::State::FAILED );
					}

					connection_->flags |= MG_F_CLOSE_IMMEDIATELY;

				}  else if ( event_ == MG_EV_CLOSE ) {

					// If a successfull response was received, a new connection needs to be made immediately (long poll),
					// if not, try the Telegram API again after the default of 5 minutes.
					if ( this->getState() == Hardware::State::READY ) {
						this->wakeUp();
					}
				}
			} ) );

		} else {
			url << "/getUpdates";

			json params = {
				{ "timeout", 60 }
			};
			if ( this->m_lastUpdateId > -1 ) {
				params["offset"] = this->m_lastUpdateId + 1;
			}

			Network::get().connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
				if ( event_ == MG_EV_HTTP_REPLY ) {
					std::string body;
					body.assign( ((http_message*)data_)->body.p, ((http_message*)data_)->body.len );
					try {
						json data = json::parse( body );
						bool success = data["ok"].get<bool>();
						if ( ! success ) {
							throw std::runtime_error( "Telegram API reported a failure" );
						}

						auto find = data.find( "result" );
						if (
							find != data.end()
							&& (*find).is_array()
						) {
							for ( auto updateIt = (*find).begin(); updateIt != (*find).end(); updateIt++ ) {
								int lastUpdateId = jsonGet<int>( *updateIt, "update_id" );
								if ( lastUpdateId > this->m_lastUpdateId ) {
									this->m_lastUpdateId = lastUpdateId;
								}

								if ( (*updateIt).find( "message" ) != (*updateIt).end() ) {
									this->_processIncomingMessage( (*updateIt)["message"] );
								}
							}
						}

					} catch( std::invalid_argument ex_ ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Invalid data received from the Telegram API." );
						this->setState( Hardware::State::FAILED );
					} catch( std::runtime_error ex_ ) {
						g_logger->log( Logger::LogLevel::ERROR, this, ex_.what() );
						this->setState( Hardware::State::FAILED );
					}

					connection_->flags |= MG_F_CLOSE_IMMEDIATELY;

				}  else if ( event_ == MG_EV_CLOSE ) {

					// If a successfull response was received, a new connection needs to be made immediately (long poll),
					// if not, try the Telegram API again after the default of 5 minutes.
					if ( this->getState() == Hardware::State::READY ) {
						this->wakeUp();
					}
				}
			} ), params );
		}

		return std::chrono::milliseconds( 1000 * 60 * 5 );
	};

	bool Telegram::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		if ( this->getState() == Hardware::State::READY ) {
			if ( device_->getType() == Device::Type::SWITCH ) {

				// This part takes care of temprarely enabling the accept new chats mode.
				std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
				if (
					device->getReference() == "accept"
					&& device->getValueOption() == Switch::Option::ACTIVATE
				) {
					if ( this->m_acceptMode ) {
						g_logger->log( Logger::LogLevel::ERROR, this, "Accept Mode already enabled." );
					} else {
						g_logger->log( Logger::LogLevel::NORMAL, this, "Accept Mode enabled." );
						this->m_acceptMode = true;
						std::thread( [this, device]{
							std::this_thread::sleep_for( std::chrono::minutes( 60 * 5 ) );
							this->m_acceptMode = false;
							device->updateValue( Device::UpdateSource::HARDWARE, Switch::Option::IDLE );
							g_logger->log( Logger::LogLevel::VERBOSE, this, "Accept Mode disabled." );
						} ).detach();
					}
					return true;
				}

			} else if ( device_->getType() == Device::Type::TEXT ) {

				// A message can be sent to a single device (=chat) or to the broadcast device, in which case it is send
				// to all chat devices.
				auto sourceDevice = std::static_pointer_cast<Text>( device_ );
				std::vector<std::shared_ptr<Text> > targetDevices;
				if ( sourceDevice->getReference() == "broadcast" ) {
					auto allDevices = this->getAllDevices();
					for ( auto deviceIt = allDevices.begin(); deviceIt != allDevices.end(); deviceIt++ ) {
						if (
							(*deviceIt)->getType() == Device::Type::TEXT
							&& (*deviceIt)->getReference() != "broadcast"
							//&& (*deviceIt)->isRunning()
						) {
							targetDevices.push_back( std::static_pointer_cast<Text>( *deviceIt ) );
						}
					}
				} else {
					targetDevices.push_back( sourceDevice );
				}
				for ( auto deviceIt = targetDevices.begin(); deviceIt != targetDevices.end(); deviceIt++ ) {
					auto targetDevice = *deviceIt;
					if ( targetDevice->getValue() != sourceDevice->getValue() ) {
						targetDevice->updateValue( Device::UpdateSource::HARDWARE, sourceDevice->getValue() );
					}
					
					std::stringstream url;
					url << "https://api.telegram.org/bot" << this->m_settings->get( "token" ) << "/sendMessage";

					json params = {
						{ "chat_id", std::stoi( targetDevice->getReference() ) },
						{ "text", sourceDevice->getValue() },
						{ "parse_mode", "Markdown" }
					};
					Network::get().connect( url.str(), Network::t_callback( [this]( mg_connection* connection_, int event_, void* data_ ) {
						if ( event_ == MG_EV_HTTP_REPLY ) {
							std::string body;
							body.assign( ((http_message*)data_)->body.p, ((http_message*)data_)->body.len );
							try {
								json data = json::parse( body );
								bool success = data["ok"].get<bool>();
								if ( ! success ) {
									throw std::runtime_error( "Telegram API reported a failure" );
								}
							} catch( std::invalid_argument ex_ ) {
								g_logger->log( Logger::LogLevel::ERROR, this, "Invalid data received from the Telegram API." );
							} catch( std::runtime_error ex_ ) {
								g_logger->log( Logger::LogLevel::ERROR, this, ex_.what() );
							}

							connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
						}
					} ), params );
				}
				return true;
			}
		} else {
			g_logger->log( Logger::LogLevel::ERROR, this, "Hardware not ready." );
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
			{ "class", this->m_settings->contains( "token" ) ? "advanced" : "normal" },
			{ "mandatory", true },
			{ "sort", 99 }
		};
		return result;
	};
	
	void Telegram::_processIncomingMessage( const json& message_ ) {
		auto find = message_.find( "chat" );
		if ( find != message_.end() ) {
			std::string reference = jsonGet<std::string>( *find, "id" );
			auto device = std::static_pointer_cast<Text>( this->getDevice( reference ) );
			if (
				device == nullptr
				&& this->m_acceptMode
			) {
				std::stringstream label;
				if ( (*find).find( "first_name" ) != (*find).end() ) {
					label << (*find)["first_name"].get<std::string>();
				}
				if ( (*find).find( "last_name" ) != (*find).end() ) {
					if ( label.str().size() > 0 ) {
						label << " ";
					}
					label << (*find)["last_name"].get<std::string>();
				}
				if ( (*find).find( "username" ) != (*find).end() ) {
					if ( label.str().size() > 0 ) {
						label << " (" << (*find)["username"].get<std::string>() << ")";
					} else {
						label << (*find)["username"].get<std::string>();
					}
				}
				if ( label.str().size() == 0 ) {
					label << reference;
				}
				device = this->declareDevice<Text>( reference, label.str(), {
					{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
					{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveSubType( Text::SubType::NOTIFICATION ) }
				} );
			}
			if (
				device != nullptr
				&& message_.find( "text" ) != message_.end()
			) {
				device->updateValue( Device::UpdateSource::HARDWARE, message_["text"].get<std::string>() );
			}
		}
	};

}; // namespace micasa
