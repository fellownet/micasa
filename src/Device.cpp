#include "Device.h"

#include "Logger.h"
#include "Hardware.h"
#include "Database.h"
#include "Controller.h"

#include "device/Counter.h"
#include "device/Level.h"
#include "device/Switch.h"
#include "device/Text.h"

namespace micasa {

	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	using namespace nlohmann;

	const std::map<Device::Type, std::string> Device::TypeText = {
		{ Device::Type::COUNTER, "counter" },
		{ Device::Type::LEVEL, "level" },
		{ Device::Type::SWITCH, "switch" },
		{ Device::Type::TEXT, "text" },
	};

	Device::Device( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : m_hardware( hardware_ ), m_id( id_ ), m_reference( reference_ ), m_label( label_ ) {
#ifdef _DEBUG
		assert( g_controller && "Global Controller instance should be created before Device instances." );
		assert( g_database && "Global Database instance should be created before Device instances." );
		assert( g_logger && "Global Logger instance should be created before Device instances." );
#endif // _DEBUG
		this->m_settings = std::make_shared<Settings<Device> >( *this );
	};
	
	Device::~Device() {
#ifdef _DEBUG
		assert( g_database && "Global Controller instance should be destroyed after Device instances." );
		assert( g_database && "Global Database instance should be destroyed after Device instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Device instances." );
#endif // _DEBUG
	};

	std::ostream& operator<<( std::ostream& out_, const Device* device_ ) {
		out_ << device_->m_hardware->getName() << " / " << device_->getName(); return out_;
	};
	
	std::string Device::getName() const {
		return this->m_settings->get( "name", this->m_label );
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
		}
	};

	template<class T> void Device::updateValue( const Device::UpdateSource& source_, const typename T::t_value& value_ ) {
		auto target = dynamic_cast<T*>( this );
#ifdef _DEBUG
		assert( target && "Invalid device template specifier." );
#endif // _DEBUG
		target->updateValue( source_, value_ );
	};
	template void Device::updateValue<Counter>( const Device::UpdateSource& source_, const Counter::t_value& value_ );
	template void Device::updateValue<Level>( const Device::UpdateSource& source_, const Level::t_value& value_ );
	template void Device::updateValue<Switch>( const Device::UpdateSource& source_, const Switch::t_value& value_ );
	template void Device::updateValue<Text>( const Device::UpdateSource& source_, const Text::t_value& value_ );
	
	template<class T> typename T::t_value Device::getValue() const {
		auto target = dynamic_cast<const T*>( this );
#ifdef _DEBUG
		assert( target && "Template specifier should match target instance." );
#endif // _DEBUG
		return target->getValue();
	};
	template Counter::t_value Device::getValue<Counter>() const;
	template Level::t_value Device::getValue<Level>() const;
	template Switch::t_value Device::getValue<Switch>() const;
	template Text::t_value Device::getValue<Text>() const;
	
	std::shared_ptr<Device> Device::factory( std::shared_ptr<Hardware> hardware_, const Type type_, const unsigned int id_, const std::string reference_, std::string label_ ) {
		switch( type_ ) {
			case Type::COUNTER:
				return std::make_shared<Counter>( hardware_, id_, reference_, label_ );
				break;
			case Type::LEVEL:
				return std::make_shared<Level>( hardware_, id_, reference_, label_ );
				break;
			case Type::SWITCH:
				return std::make_shared<Switch>( hardware_, id_, reference_, label_ );
				break;
			case Type::TEXT:
				return std::make_shared<Text>( hardware_, id_, reference_, label_ );
				break;
		}
		return nullptr;
	};

	json Device::getJson( bool full_ ) const {
		json result = this->m_hardware->getDeviceJson( this->shared_from_this() );
		result["id"] = this->m_id;
		result["label"] = this->getLabel();
		result["name"] = this->getName();
		result["enabled"] = this->isRunning();
		//result["hardware"] = this->m_hardware->getJson( false );
		result["hardware"] = this->m_hardware->getName();
		result["hardware_id"] = this->m_hardware->getId();
		result["scheduled"] = g_controller->isScheduled( this->shared_from_this() );
		result["ignore_duplicates"] = this->getSettings()->get<bool>( "ignore_duplicates", this->getType() == Device::Type::SWITCH || this->getType() == Device::Type::TEXT );

		if ( this->getSettings()->contains( DEVICE_SETTING_BATTERY_LEVEL ) ) {
			result["battery_level"] = this->getSettings()->get<unsigned int>( DEVICE_SETTING_BATTERY_LEVEL );
		}
		if ( this->getSettings()->contains( DEVICE_SETTING_SIGNAL_STRENGTH ) ) {
			result["signal_strength"] = this->getSettings()->get<unsigned int>( DEVICE_SETTING_SIGNAL_STRENGTH );
		}

		// TODO the webserver should not call getJson but query the database directly.
		json timeProperties = g_database->getQueryRow<json>(
			"SELECT CAST(strftime('%%s',`updated`) AS INTEGER) AS `last_update`, CAST(strftime('%%s','now') AS INTEGER) - CAST(strftime('%%s',`updated`) AS INTEGER) AS `age` "
			"FROM `devices` "
			"WHERE `id`=%d ",
			this->getId()
		);
		result["last_update"] = timeProperties["last_update"].get<unsigned long>();
		result["age"] = timeProperties["age"].get<unsigned long>();
			
		result["total_timers"] = g_database->getQueryValue<unsigned int>(
			"SELECT COUNT(`timer_id`) "
			"FROM `x_timer_devices` "
			"WHERE `device_id`=%d ",
			this->getId()
		);
		result["total_scripts"] = g_database->getQueryValue<unsigned int>(
			"SELECT COUNT(`script_id`) "
			"FROM `x_device_scripts` "
			"WHERE `device_id`=%d ",
			this->getId()
		);
		result["total_links"] = g_database->getQueryValue<unsigned int>(
			"SELECT COUNT(`id`) "
			"FROM `links` "
			"WHERE `device_id`=%d "
			"OR `target_device_id`=%d",
			this->getId(),
			this->getId()
		);

		auto updateSources = this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES );
		result["readonly"] = ( Device::resolveUpdateSource( updateSources & ( Device::UpdateSource::TIMER | Device::UpdateSource::SCRIPT | Device::UpdateSource::API ) ) == 0 );

		return result;
	};

	json Device::getSettingsJson() const {
		json result = this->m_hardware->getDeviceSettingsJson( this->shared_from_this() );

		json setting = {
			{ "name", "name" },
			{ "label", "Name" },
			{ "type", "string" },
			{ "maxlength", 64 },
			{ "minlength", 3 },
			{ "mandatory", true },
			{ "sort", 1 }
		};
		result += setting;

		setting = {
			{ "name", "enabled" },
			{ "label", "Enabled" },
			{ "type", "boolean" },
			{ "default", true },
			{ "sort", 2 }
		};
		result += setting;

		setting = {
			{ "name", "ignore_duplicates" },
			{ "label", "Ignore Duplicates" },
			{ "description", "When this checkbox is enabled all duplicate values received for this device are discarded." },
			{ "type", "boolean" },
			{ "class", "advanced" },
			{ "default", this->getType() == Device::Type::SWITCH || this->getType() == Device::Type::TEXT },
			{ "sort", 3 }
		};
		result += setting;

		return result;
	};

	void Device::putSettingsJson( json& settings_ ) {
		this->m_hardware->putDeviceSettingsJson( this->shared_from_this(), settings_ );
	};

	void Device::touch() {
		g_database->putQuery(
			"UPDATE `devices` "
			"SET `updated`=datetime('now') "
			"WHERE `id`=%d ",
			this->getId()
		);
		this->m_hardware->touch();
	};
	
	void Device::setScripts( std::vector<unsigned int>& scriptIds_ ) {
		std::stringstream list;
		unsigned int index = 0;
		for ( auto scriptIdsIt = scriptIds_.begin(); scriptIdsIt != scriptIds_.end(); scriptIdsIt++ ) {
			list << ( index > 0 ? "," : "" ) << (*scriptIdsIt);
			index++;
			g_database->putQuery(
				"INSERT OR IGNORE INTO `x_device_scripts` "
				"(`device_id`, `script_id`) "
				"VALUES (%d, %d)",
				this->getId(),
				(*scriptIdsIt)
			);
		}
		g_database->putQuery(
			"DELETE FROM `x_device_scripts` "
			"WHERE `device_id`=%d "
			"AND `script_id` NOT IN (%q)",
			this->getId(),
			list.str().c_str()
		);
	};

}; // namespace micasa
