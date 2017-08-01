#include "Dummy.h"

#include "../Logger.h"
#include "../User.h"
#include "../Utils.h"
#include "../device/Counter.h"
#include "../device/Level.h"
#include "../device/Text.h"
#include "../device/Switch.h"

namespace micasa {

	const char* Dummy::label = "Dummy";

	void Dummy::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Plugin::start();

		this->declareDevice<Switch>( "create_switch_device", "Add Switch Device", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::PLUGIN, Switch::Option::IDLE );
		this->declareDevice<Switch>( "create_level_device", "Add Level Device", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::PLUGIN, Switch::Option::IDLE );
		this->declareDevice<Switch>( "create_counter_device", "Add Counter Device", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::PLUGIN, Switch::Option::IDLE );
		this->declareDevice<Switch>( "create_text_device", "Add Text Device", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::PLUGIN | Device::UpdateSource::API ) },
			{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::ACTION ) },
			{ DEVICE_SETTING_MINIMUM_USER_RIGHTS,    User::resolveRights( User::Rights::INSTALLER ) }
		} )->updateValue( Device::UpdateSource::PLUGIN, Switch::Option::IDLE );

		this->setState( Plugin::State::READY );
	};

	void Dummy::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Plugin::stop();
	};

	bool Dummy::updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) {
		if ( device_->getType() == Device::Type::SWITCH ) {
			std::shared_ptr<Switch> device = std::static_pointer_cast<Switch>( device_ );
			if ( device->getValueOption() == Switch::Option::ACTIVATE ) {
				std::string type = device->getReference();
				if ( type == "create_switch_device" ) {
					auto device = this->declareDevice<Switch>( randomString( 16 ), "Switch", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Switch::resolveTextSubType( Switch::SubType::GENERIC ) },
						{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
						{ DEVICE_SETTING_ADDED_MANUALLY,         true }
					} );
					device->setEnabled( true );
					device->start();
					device->updateValue( Device::UpdateSource::PLUGIN, Switch::Option::OFF );
				} else if ( type == "create_counter_device" ) {
					auto device = this->declareDevice<Counter>( randomString( 16 ), "Counter", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Counter::resolveTextSubType( Counter::SubType::GENERIC ) },
						{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Counter::resolveTextUnit( Counter::Unit::GENERIC ) },
						{ DEVICE_SETTING_ALLOW_UNIT_CHANGE,      true },
						{ DEVICE_SETTING_ADDED_MANUALLY,         true }
					} );
					device->setEnabled( true );
					device->start();
					device->updateValue( Device::UpdateSource::PLUGIN, 0 );
				} else if ( type == "create_level_device" ) {
					auto device = this->declareDevice<Level>( randomString( 16 ), "Level", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Level::resolveTextSubType( Level::SubType::GENERIC ) },
						{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
						{ DEVICE_SETTING_DEFAULT_UNIT,           Level::resolveTextUnit( Level::Unit::GENERIC ) },
						{ DEVICE_SETTING_ALLOW_UNIT_CHANGE,      true },
						{ DEVICE_SETTING_ADDED_MANUALLY,         true }
					} );
					device->setEnabled( true );
					device->start();
					device->updateValue( Device::UpdateSource::PLUGIN, 0. );
				} else if ( type == "create_text_device" ) {
					auto device = this->declareDevice<Text>( randomString( 16 ), "Text", {
						{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::ANY ) },
						{ DEVICE_SETTING_DEFAULT_SUBTYPE,        Text::resolveTextSubType( Text::SubType::GENERIC ) },
						{ DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE,   true },
						{ DEVICE_SETTING_ADDED_MANUALLY,         true }
					} );
					device->setEnabled( true );
					device->start();
					device->updateValue( Device::UpdateSource::PLUGIN, "" );
				}
			}
		}
		return true;
	};

}; // namespace micasa
