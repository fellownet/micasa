#include "Device.h"

#include "Hardware.h"
#include "Database.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	Device::Device( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : m_hardware( hardware_ ), m_id( id_ ), m_reference( reference_ ), m_label( label_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Device instances." );
		assert( g_webServer && "Global Database instance should be created before Device instances." );
		assert( g_logger && "Global Logger instance should be created before Device instances." );
#endif // _DEBUG
		this->m_settings.populate( *this );
		this->m_lastUpdate = std::chrono::system_clock::now();
	};
	
	Device::~Device() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Device instances." );
		assert( g_webServer && "Global Database instance should be destroyed after Device instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Device instances." );
#endif // _DEBUG
	};

	std::ostream& operator<<( std::ostream& out_, const Device* device_ ) {
		out_ << device_->m_hardware->getName() << " / " << device_->getName(); return out_;
	};
	
	void Device::start() {
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				if ( output_.is_null() ) {
					output_ = nlohmann::json::array();
				}
				output_ += this->getJson();
			} )
		} ) ) );

		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( this->m_id ),
			"api/devices/" + std::to_string( this->m_id ),
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			WebServer::t_callback( [this]( const std::string& uri_, const nlohmann::json& input_, const WebServer::Method& method_, int& code_, nlohmann::json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = this->getJson();
						break;
					}
						
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {
						try {
							bool success = true;
							if ( input_.find( "name" ) != input_.end() ) {
								this->m_settings.put( "name", input_["name"].get<std::string>() );
								this->m_settings.commit( *this );
							}
							if ( input_.find( "value" ) != input_.end() ) {
								switch( this->getType() ) {
									case COUNTER:
										if ( input_["value"].is_string() ) {
											success = success && this->updateValue<Counter>( Device::UpdateSource::API, std::stoi( input_["value"].get<std::string>() ) );
										} else if ( input_["value"].is_number() ) {
											success = success && this->updateValue<Counter>( Device::UpdateSource::API, input_["value"].get<int>() );
										} else {
											success = false;
											output_["message"] = "Invalid value.";
										}
										break;
									case LEVEL:
										if ( input_["value"].is_string() ) {
											success = success && this->updateValue<Level>( Device::UpdateSource::API, std::stof( input_["value"].get<std::string>() ) );
										} else if ( input_["value"].is_number() ) {
											success = success && this->updateValue<Level>( Device::UpdateSource::API, input_["value"].get<double>() );
										} else {
											success = false;
											output_["message"] = "Invalid value.";
										}
										break;
									case SWITCH:
										success = success && this->updateValue<Switch>( Device::UpdateSource::API, input_["value"].get<std::string>() );
										break;
									case TEXT:
										success = success && this->updateValue<Text>( Device::UpdateSource::API, input_["value"].get<std::string>() );
										break;
								}
							}
							if ( success ) {
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
							}
							g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to update device.";
							code_ = 400; // bad request
						}
						break;
					}
					case WebServer::Method::DELETE: {
						output_["result"] = "ERROR";
						output_["message"] = "Deleting is not implemented.";
						code_ = 501; // not implemented
						break;
					}
					default: break;
				}
			} )
		} ) ) );

		Worker::start();
	};
	
	void Device::stop() {
		g_webServer->removeResourceCallback( "device-" + std::to_string( this->m_id ) );
		if ( this->m_settings.isDirty() ) {
			this->m_settings.commit( *this );
		}
		Worker::stop();
	};
	
	const std::string Device::getName() const {
		return this->m_settings.get( "name", this->m_label );
	};

	void Device::setLabel( const std::string& label_ ) {
		if ( label_ != this->m_label ) {
			this->m_label = label_;
			g_database->putQuery(
				"UPDATE `devices` "
				"SET `label`=%Q "
				"WHERE `id`=%d"
				, label_.c_str(), this->m_id
			);
			g_webServer->touchResourceCallback( "device-" + std::to_string( this->m_id ) );
		}
	};

	template<class T> bool Device::updateValue( const unsigned int& source_, const typename T::t_value& value_ ) {
		auto target = dynamic_cast<T*>( this );
#ifdef _DEBUG
		assert( target && "Invalid device template specifier." );
#endif // _DEBUG
		return target->updateValue( source_, value_ );
	};
	template bool Device::updateValue<Counter>( const unsigned int& source_, const Counter::t_value& value_ );
	template bool Device::updateValue<Level>( const unsigned int& source_, const Level::t_value& value_ );
	template bool Device::updateValue<Switch>( const unsigned int& source_, const Switch::t_value& value_ );
	template bool Device::updateValue<Text>( const unsigned int& source_, const Text::t_value& value_ );
	
	template<class T> const typename T::t_value& Device::getValue() const {
		auto target = dynamic_cast<const T*>( this );
#ifdef _DEBUG
		assert( target && "Template specifier should match target instance." );
#endif // _DEBUG
		return target->getValue();
	};
	template const Counter::t_value& Device::getValue<Counter>() const;
	template const Level::t_value& Device::getValue<Level>() const;
	template const Switch::t_value& Device::getValue<Switch>() const;
	template const Text::t_value& Device::getValue<Text>() const;
	
	std::shared_ptr<Device> Device::_factory( std::shared_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_ ) {
		switch( type_ ) {
			case COUNTER:
				return std::make_shared<Counter>( hardware_, id_, reference_, label_ );
				break;
			case LEVEL:
				return std::make_shared<Level>( hardware_, id_, reference_, label_ );
				break;
			case SWITCH:
				return std::make_shared<Switch>( hardware_, id_, reference_, label_ );
				break;
			case TEXT:
				return std::make_shared<Text>( hardware_, id_, reference_, label_ );
				break;
		}
		return nullptr;
	};

	json Device::getJson() const {
		auto age = std::chrono::system_clock::now() - this->m_lastUpdate;
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "hardware", this->m_hardware->getJson() },
			// TODO the age on resources on the webpage doesn't update if the device doesn't, so lastUpdate might
			// be better somehow.
			{ "age", std::chrono::duration_cast<std::chrono::milliseconds>( age ).count() / 1000. }
		};
		return result;
	};

}; // namespace micasa
