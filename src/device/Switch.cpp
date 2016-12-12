#include "Switch.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	const std::map<Switch::Option, std::string> Switch::OptionText = {
		{ Switch::Option::ON, "On" },
		{ Switch::Option::OFF, "Off" },
		{ Switch::Option::OPEN, "Open" },
		{ Switch::Option::CLOSED, "Closed" },
		{ Switch::Option::STOPPED, "Stopped" },
		{ Switch::Option::STARTED, "Started" },
	};
	
	void Switch::start() {
		std::string value = g_database->getQueryValue<std::string>(
			"SELECT `value` "
			"FROM `device_switch_history` "
			"WHERE `device_id`=%d "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id
		);
		this->m_value = (Option)atoi( value.c_str() );
		
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string& uri_, const std::map<std::string, std::string>& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( output_.is_null() ) {
					output_ = nlohmann::json::array();
				}
				auto inputIt = input_.find( "hardware_id" );
				if (
					inputIt == input_.end()
					|| (*inputIt).second == std::to_string( this->m_hardware->getId() )
				) {
					auto json = this->getJson();
					json["value"] = Switch::OptionText.at( this->m_value );
					output_ += json;
				}
			} )
		} ) ) );
		
		// If the switch can be operated by the user through the API (defined by the hardware) an additionl method
		// should be added.
		unsigned int methods = WebServer::Method::GET;
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & Device::UpdateSource::API ) == Device::UpdateSource::API ) {
			methods |= WebServer::Method::PATCH;
		}
		
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices/" + std::to_string( this->m_id ),
			methods,
			WebServer::t_callback( [this]( const std::string& uri_, const std::map<std::string, std::string>& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						auto json = this->getJson();
						json["value"] = Switch::OptionText.at( this->m_value );
						output_ = json;
						break;
					}
					case WebServer::Method::PATCH:
						// TODO implement patch method, for now it toggles between on and off.
						// TODO pass error and set code on failure, but what message and what code?
						if ( this->m_value == Switch::Option::ON ) {
							output_["result"] = this->updateValue( Device::UpdateSource::API, Switch::Option::OFF ) ? "OK" : "ERROR";
						} else {
							output_["result"] = this->updateValue( Device::UpdateSource::API, Switch::Option::ON ) ? "OK" : "ERROR";
						}
						break;
					default:
						g_logger->log( Logger::LogLevel::ERROR, this, "Invalid API method." );
						break;
				}
			} )
		} ) ) );
		
		Device::start();
	};

	void Switch::stop() {
		g_webServer->removeResourceCallback( "device-" + std::to_string( this->m_id ) );
		Device::stop();
	};

	bool Switch::updateValue( const unsigned int& source_, const Option& value_ ) {
#ifdef _DEBUG
		assert( Switch::OptionText.find( value_ ) != Switch::OptionText.end() && "Switch should be defined." );
#endif // _DEBUG
		
	   // The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}

		// Make a local backup of the original value (the hardware might want to revert it).
		Option currentValue = this->m_value;
		this->m_value = value_;
		
		// If the update originates from the hardware, do not send it to the hardware again!
		bool success = true;
		bool apply = true;
		if ( ( source_ & Device::UpdateSource::HARDWARE ) != Device::UpdateSource::HARDWARE ) {
			success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		}
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_switch_history` (`device_id`, `value`) "
				"VALUES (%d, %d)"
				, this->m_id, (unsigned int)value_
			);
			g_controller->newEvent<Switch>( *this, source_ );
			g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", Switch::OptionText.at( value_ ).c_str() );
		} else {
			this->m_value = currentValue;
		}
		return success;
	};
	
	bool Switch::updateValue( const unsigned int& source_, const t_value& value_ ) {
		for ( auto optionsIt = OptionText.begin(); optionsIt != OptionText.end(); optionsIt++ ) {
			if ( optionsIt->second == value_ ) {
				return this->updateValue( source_, optionsIt->first );
			}
		}
		return false;
	};

	const std::chrono::milliseconds Switch::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for switch devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_switch_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings.get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
		}
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	};
	
}; // namespace micasa
