#include "Switch.h"

#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Controller> g_controller;

	const std::map<int, std::string> Switch::OptionsText = {
		{ Switch::Options::ON, "On" },
		{ Switch::Options::OFF, "Off" },
		{ Switch::Options::OPEN, "Open" },
		{ Switch::Options::CLOSED, "Closed" },
		{ Switch::Options::STOPPED, "Stopped" },
		{ Switch::Options::STARTED, "Started" },
	};
	
	void Switch::start() {
		std::string value = g_database->getQueryValue<std::string>(
			"SELECT `value` "
			"FROM `device_switch_history` "
			"WHERE `device_id`=%q "
			"ORDER BY `date` DESC "
			"LIMIT 1"
			, this->m_id.c_str()
		);
		this->m_value = atoi( value.c_str() );
		
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + this->m_id,
			"Returns a list of available devices.",
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string uri_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				output_ += {
					{ "id", atoi( this->m_id.c_str() ) },
					{ "name", this->m_name },
					{ "value", Switch::OptionsText.at( this->m_value ) }
				};
			} )
		} ) ) );
		
		// If the switch can be operated by the user through the API (defined by the hardware) an additionl method
		// should be added.
		unsigned int methods = WebServer::Method::GET;
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & Device::UpdateSource::API ) == Device::UpdateSource::API ) {
			methods |= WebServer::Method::PATCH;
		}
		
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + this->m_id,
			"Returns detailed information for " + this->m_name,
			"api/devices/" + this->m_id,
			methods,
			WebServer::t_callback( [this]( const std::string uri_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET:
						output_["id"] = atoi( this->m_id.c_str() );
						output_["name"] = this->m_name;
						output_["value"] = Switch::OptionsText.at( this->m_value );
						break;
					case WebServer::Method::PATCH:
						// TODO implement patch method, for now it toggles between on and off.
						if ( this->m_value == Switch::Options::ON ) {
							output_["result"] = this->updateValue( Device::UpdateSource::API, Switch::Options::OFF ) ? "OK" : "ERROR";
						} else {
							output_["result"] = this->updateValue( Device::UpdateSource::API, Switch::Options::ON ) ? "OK" : "ERROR";
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
		g_webServer->removeResourceCallback( "device-" + this->m_id );
		Device::stop();
	};

	bool Switch::updateValue( const Device::UpdateSource source_, const Options value_ ) {
#ifdef _DEBUG
		assert( Switch::OptionsText.find( value_ ) != Switch::OptionsText.end() && "Switch should be defined." );
#endif // _DEBUG
		
	   // The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings.get<unsigned int>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, 0 ) & source_ ) != source_ ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Invalid update source." );
			return false;
		}

		bool apply = true;
		unsigned int currentValue = this->m_value;
		this->m_value = value_;
		bool success = this->m_hardware->updateDevice( source_, this->shared_from_this(), apply );
		if ( success && apply ) {
			g_database->putQuery(
				"INSERT INTO `device_switch_history` (`device_id`, `value`) "
				"VALUES (%q, %d)"
				, this->m_id.c_str(), (unsigned int)value_
			);
			g_controller->newEvent<Switch>( *this, source_ );
			g_webServer->touchResourceAt( "api/devices" );
			g_webServer->touchResourceAt( "api/devices/" + this->m_id );
			g_logger->logr( Logger::LogLevel::NORMAL, this, "New value %s.", Switch::OptionsText.at( value_ ).c_str() );
		} else {
			this->m_value = currentValue;
		}
		return success;
	}

	std::chrono::milliseconds Switch::_work( const unsigned long int iteration_ ) {
		if ( iteration_ > 0 ) {
			// Purge history after a configured period (defaults to 31 days for switch devices because these
			// lack a separate trends table).
			g_database->putQuery(
				"DELETE FROM `device_switch_history` "
				"WHERE `device_id`=%q AND `Date` < datetime('now','-%d day')"
				, this->m_id.c_str(), this->m_settings.get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
		}
		return std::chrono::milliseconds( 1000 * 60 * 60 );
	}
	
}; // namespace micasa
