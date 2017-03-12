#include "Text.h"

#include "../Logger.h"
#include "../Database.h"
#include "../Hardware.h"
#include "../Controller.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Controller> g_controller;

	using namespace std::chrono;
	using namespace nlohmann;

	const Device::Type Text::type = Device::Type::TEXT;

	const std::map<Text::SubType, std::string> Text::SubTypeText = {
		{ Text::SubType::GENERIC, "generic" },
		{ Text::SubType::WIND_DIRECTION, "wind_direction" },
		{ Text::SubType::NOTIFICATION, "notification" }
	};

	Text::Text( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) {
		try {
			this->m_value = g_database->getQueryValue<std::string>(
				"SELECT `value` "
				"FROM `device_text_history` "
				"WHERE `device_id`=%d "
				"ORDER BY `date` DESC "
				"LIMIT 1",
				this->m_id
			);
		} catch( const Database::NoResultsException& ex_ ) {
			g_logger->log( Logger::LogLevel::DEBUG, this, "No starting value." );
		}
	};

	void Text::updateValue( const Device::UpdateSource& source_, const t_value& value_ ) {
		
		// The update source should be defined in settings by the declaring hardware.
		if ( ( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) & source_ ) != source_ ) {
			auto configured = Device::resolveUpdateSource( this->m_settings->get<Device::UpdateSource>( DEVICE_SETTING_ALLOWED_UPDATE_SOURCES ) );
			auto requested = Device::resolveUpdateSource( source_ );
			g_logger->logr( Logger::LogLevel::ERROR, this, "Invalid update source (%d vs %d).", configured, requested );
			return;
		}

		if (
			this->getSettings()->get<bool>( "ignore_duplicates", this->getType() == Device::Type::SWITCH || this->getType() == Device::Type::TEXT )
			&& this->m_value == value_
			&& static_cast<unsigned short>( source_ & Device::UpdateSource::INIT ) == 0
		) {
			g_logger->log( Logger::LogLevel::VERBOSE, this, "Ignoring duplicate value." );
			return;
		}
		
		auto fAction = []( std::shared_ptr<Text> me_, const Device::UpdateSource& source_, const t_value& value_ ) {

			// Make a local backup of the original value (the hardware might want to revert it).
			t_value previous = me_->m_value;
			me_->m_value = value_;
			
			// If the update originates from the hardware, or the value is NOT different than the current value,
			// do not send it to the hardware again.
			bool success = true;
			bool apply = true;
			if ( ( source_ & Device::UpdateSource::HARDWARE ) != Device::UpdateSource::HARDWARE ) {
				success = me_->m_hardware->updateDevice( source_, me_, apply );
			}
			if ( success && apply ) {
				g_database->putQueryAsync(
					"INSERT INTO `device_text_history` (`device_id`, `value`) "
					"VALUES (%d, %Q)",
					me_->m_id,
					me_->m_value.c_str()
				);
				me_->m_previousValue = previous; // before newEvent so previous value can be provided
				if ( me_->isRunning() ) {
					g_controller->newEvent<Text>( me_, source_ );
				}
				g_logger->logr( me_->isRunning() ? Logger::LogLevel::NORMAL : Logger::LogLevel::DEBUG, me_, "New value %s.", me_->m_value.c_str() );
			} else {
				me_->m_value = previous;
			}
			if (
				success
				&& ( source_ & Device::UpdateSource::INIT ) != Device::UpdateSource::INIT
			) {
				me_->touch();
			}
		};

		std::shared_ptr<Text> me = std::static_pointer_cast<Text>( this->shared_from_this() );
		if ( this->m_settings->contains( "rate_limit" ) ) {
			std::thread( [me,source_,value_,fAction]() {
				me->m_rateLimiter.value = value_;
				unsigned long duration = 1000 * me->m_settings->get<double>( "rate_limit" );
				if ( me->m_rateLimiter.trying ) {
					return;
				}
				me->m_rateLimiter.trying = true;
				if ( me->m_rateLimiter.mutex.try_lock_for( milliseconds( duration ) ) ) {
					me->m_rateLimiter.trying = false;
					fAction( me, source_, me->m_rateLimiter.value );
					std::this_thread::sleep_for( milliseconds( duration ) );
					me->m_rateLimiter.mutex.unlock();
				} else {
					me->m_rateLimiter.trying = false;
				}
			} ).detach();
		} else {
			fAction( me, source_, value_ );
		}
	};

	json Text::getJson( bool full_ ) const {
		json result = Device::getJson( full_ );
		result["value"] = this->getValue();
		result["type"] = "text";
		result["subtype"] = this->m_settings->get( "subtype", this->m_settings->get( DEVICE_SETTING_DEFAULT_SUBTYPE, "" ) );
		if ( this->m_settings->contains( "rate_limit" ) ) {
			result["rate_limit"] = this->m_settings->get<double>( "rate_limit" );
		}
		
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		
		return result;
	};

	json Text::getSettingsJson() const {
		json result = Device::getSettingsJson();
		if ( this->m_settings->get<bool>( DEVICE_SETTING_ALLOW_SUBTYPE_CHANGE, false ) ) {
			json setting = {
				{ "name", "subtype" },
				{ "label", "SubType" },
				{ "type", "list" },
				{ "options", json::array() },
				{ "class", this->m_settings->contains( "subtype" ) ? "advanced" : "normal" },
				{ "sort", 10 }
			};
			for ( auto subTypeIt = Text::SubTypeText.begin(); subTypeIt != Text::SubTypeText.end(); subTypeIt++ ) {
				setting["options"] += {
					{ "value", subTypeIt->second },
					{ "label", subTypeIt->second }
				};
			}
			result += setting;
		}

		result += {
			{ "name", "rate_limit" },
			{ "label", "Rate Limiter" },
			{ "description", "Limits the number of updates to once per configured time period in seconds." },
			{ "type", "double" },
			{ "class", "advanced" },
			{ "sort", 999 }
		};

		return result;
	};
	
	json Text::getData( unsigned int range_, const std::string& interval_ ) const {
		std::vector<std::string> validIntervals = { "day", "week", "month", "year" };
		if ( std::find( validIntervals.begin(), validIntervals.end(), interval_ ) == validIntervals.end() ) {
			return json::array();
		}
		std::string interval = interval_;
		if ( interval == "week" ) {
			interval = "day";
			range_ *= 7;
		}

		return g_database->getQuery<json>(
			"SELECT `value`, CAST(strftime('%%s',`date`) AS INTEGER) AS `timestamp` "
			"FROM `device_text_history` "
			"WHERE `device_id`=%d "
			"AND `date` >= datetime('now','-%d %s') "
			"ORDER BY `date` ASC ",
			this->m_id,
			range_,
			interval.c_str()
		);
	};
	
	milliseconds Text::_work( const unsigned long int& iteration_ ) {
		if ( iteration_ > 0 ) {

			// Purge history after a configured period (defaults to 31 days for text devices because these
			// lack a separate trends table).
			g_database->putQueryAsync(
				"DELETE FROM `device_text_history` "
				"WHERE `device_id`=%d AND `Date` < datetime('now','-%d day')"
				, this->m_id, this->m_settings->get<int>( DEVICE_SETTING_KEEP_HISTORY_PERIOD, 31 )
			);
			return milliseconds( 1000 * 60 * 60 );

		} else {

			// To prevent all devices from crunching data at the same time an offset is used.
			static volatile unsigned int offset = 0;
			offset += ( 1000 * 25 ); // 25 seconds interval
			return milliseconds( offset % ( 1000 * 60 * 5 ) );
		}
	};
	
}; // namespace micasa
