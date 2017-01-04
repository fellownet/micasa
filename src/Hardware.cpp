#include <iostream>

#include "Hardware.h"
#include "Database.h"
#include "Controller.h"
#include "Utils.h"

#ifdef _WITH_OPENZWAVE
	#include "hardware/OpenZWave.h"
	#include "hardware/OpenZWaveNode.h"
#endif // _WITH_OPENZWAVE

#include "hardware/WeatherUnderground.h"
#include "hardware/PiFace.h"
#include "hardware/PiFaceBoard.h"
#include "hardware/HarmonyHub.h"
#include "hardware/P1Meter.h"
#include "hardware/RFXCom.h"
#include "hardware/SolarEdge.h"
#include "hardware/SolarEdgeInverter.h"
#include "hardware/Dummy.h"

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	const std::map<Hardware::Type, std::string> Hardware::TypeText = {
		{ Hardware::Type::HARMONY_HUB, "harmony_hub" },
#ifdef _WITH_OPENZWAVE
		{ Hardware::Type::OPEN_ZWAVE, "openzwave" },
		{ Hardware::Type::OPEN_ZWAVE_NODE, "openzwave_node" },
#endif // _WITH_OPENZWAVE
		{ Hardware::Type::P1_METER, "p1_meter" },
		{ Hardware::Type::PIFACE, "piface" },
		{ Hardware::Type::PIFACE_BOARD, "piface_board" },
		{ Hardware::Type::RFXCOM, "rfxcom" },
		{ Hardware::Type::SOLAREDGE, "solaredge" },
		{ Hardware::Type::SOLAREDGE_INVERTER, "solaredge_inverter" },
		{ Hardware::Type::WEATHER_UNDERGROUND, "weather_underground" },
		{ Hardware::Type::DUMMY, "dummy" }
	};

	const std::map<Hardware::State, std::string> Hardware::StateText = {
		{ Hardware::State::INIT, "initializing" },
		{ Hardware::State::READY, "ready" },
		{ Hardware::State::DISABLED, "disabled" },
		{ Hardware::State::FAILED, "failed" },
		{ Hardware::State::SLEEPING, "sleeping" }
	};

	Hardware::Hardware( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Worker(), m_id( id_ ), m_type( type_ ), m_reference( reference_ ), m_parent( parent_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Hardware instances." );
		assert( g_webServer && "Global Database instance should be created before Hardware instances." );
		assert( g_logger && "Global Logger instance should be created before Hardware instances." );
#endif // _DEBUG
		this->m_settings.populate( *this );
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Hardware instances." );
		assert( g_webServer && "Global Database instance should be destroyed after Hardware instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Hardware instances." );
#endif // _DEBUG
	};

	std::shared_ptr<Hardware> Hardware::_factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) {
		switch( type_ ) {
			case HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, type_, reference_, parent_ );
				break;
#ifdef _WITH_OPENZWAVE
			case OPEN_ZWAVE:
				return std::make_shared<OpenZWave>( id_, type_, reference_, parent_ );
				break;
			case OPEN_ZWAVE_NODE:
				return std::make_shared<OpenZWaveNode>( id_, type_, reference_, parent_ );
				break;
#endif // _WITH_OPENZWAVE
			case P1_METER:
				return std::make_shared<P1Meter>( id_, type_, reference_, parent_ );
				break;
			case PIFACE:
				return std::make_shared<PiFace>( id_, type_, reference_, parent_ );
				break;
			case PIFACE_BOARD:
				return std::make_shared<PiFaceBoard>( id_, type_, reference_, parent_ );
				break;
			case RFXCOM:
				return std::make_shared<RFXCom>( id_, type_, reference_, parent_ );
				break;
			case SOLAREDGE:
				return std::make_shared<SolarEdge>( id_, type_, reference_, parent_ );
				break;
			case SOLAREDGE_INVERTER:
				return std::make_shared<SolarEdgeInverter>( id_, type_, reference_, parent_ );
				break;
			case WEATHER_UNDERGROUND:
				return std::make_shared<WeatherUnderground>( id_, type_, reference_, parent_ );
				break;
			case DUMMY:
				return std::make_shared<Dummy>( id_, type_, reference_, parent_ );
				break;
		}
#ifdef _DEBUG
		assert( true && "Hardware types should be defined in the Type enum." );
#endif // _DEBUG
		return nullptr;
	}

	void Hardware::start() {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );

		// Fetch all device records from the database, including those that are disabled. Disabled devices are *not*
		// started but are still present in the devices vector.
		std::vector<std::map<std::string, std::string> > devicesData = g_database->getQuery(
			"SELECT `id`, `reference`, `label`, `type`, `enabled` "
			"FROM `devices` "
			"WHERE `hardware_id`=%d"
			, this->m_id
		);
		for ( auto devicesIt = devicesData.begin(); devicesIt != devicesData.end(); devicesIt++ ) {
			Device::Type type = static_cast<Device::Type>( atoi( (*devicesIt)["type"].c_str() ) );
#ifdef _DEBUG
			assert( type >= 1 && type <= 4 && "Device types should be defined in the Type enum." );
#endif // _DEBUG
			std::shared_ptr<Device> device = Device::_factory( this->shared_from_this(), type, std::stoi( (*devicesIt)["id"] ), (*devicesIt)["reference"], (*devicesIt)["label"] );

			if ( (*devicesIt)["enabled"] == "1" ) {
				device->start();
			}
			this->_installDeviceResourceHandlers( device );

			this->m_devices.push_back( device );
		}

		// Set the state to initializing if the hardware itself didn't already set the state to something else.
		if ( this->getState() == DISABLED ) {
			this->_setState( INIT );
		}
		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Hardware::stop() {
		{
			std::lock_guard<std::mutex> lock( this->m_devicesMutex );
			for( auto devicesIt = this->m_devices.begin(); devicesIt < this->m_devices.end(); devicesIt++ ) {
				auto device = (*devicesIt);
				g_webServer->removeResourceCallback( "device-" + std::to_string( device->getId() ) );
				if ( device->isRunning() ) {
					device->stop();
				}
			}
			this->m_devices.clear();
		}

		if ( this->m_settings.isDirty() ) {
			this->m_settings.commit( *this );
		}

		this->_setState( DISABLED );
		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	const Hardware::Type Hardware::getType() const {
		return this->m_type;
	};
	template<> const unsigned int Hardware::getType() const {
		return (unsigned int)this->m_type;
	};
	template<> const std::string Hardware::getType() const {
		return Hardware::TypeText.at( this->getType() );
	};

	const Hardware::State Hardware::getState() const {
		return this->m_state;
	};
	template<> const unsigned int Hardware::getState() const {
		return (unsigned int)this->m_state;
	};
	template<> const std::string Hardware::getState() const {
		return Hardware::StateText.at( this->getState() );
	};

	const std::string Hardware::getName() const {
		return this->m_settings.get( "name", this->getLabel() );
	};

	void Hardware::setSettings( const std::map<std::string,std::string>& settings_ ) {
		std::vector<std::string> allowedSettings;
		stringSplit( this->m_settings.get( HARDWARE_SETTINGS_ALLOWED, "" ), ',', allowedSettings );
		for ( auto allowedSettingsIt = allowedSettings.begin(); allowedSettingsIt != allowedSettings.end(); allowedSettingsIt++ ) {
			if ( settings_.count( *allowedSettingsIt ) > 0 ) {
				this->m_settings.put( *allowedSettingsIt, settings_.at( *allowedSettingsIt ) );
			}
		}
		if ( this->m_settings.isDirty() ) {
			this->m_settings.commit( *this );
		}
	};

	void Hardware::_setState( const State& state_ ) {
		if ( this->m_state != state_ ) {
			g_webServer->touchResourceCallback( "hardware-" + std::to_string( this->m_id ) );
			this->m_state = state_;
		}
	};

	json Hardware::getJson() const {
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "enabled", this->isRunning() },
			{ "type", this->getType<std::string>() },
			{ "state", this->getState<std::string>() },
			{ "settings", this->m_settings.getAll( this->m_settings.get( HARDWARE_SETTINGS_ALLOWED, "" ), true ) }
		};
		if ( this->m_parent ) {
			result["parent"] = this->m_parent->getJson();
		}
		return result;
	};

	std::shared_ptr<Device> Hardware::_getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::_getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getId() == id_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::_getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getName() == name_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::_getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getLabel() == label_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	template<class T> std::shared_ptr<T> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_, const bool& start_ ) {
		// TODO also declare relationships with other devices, such as energy and power, or temperature
		// and humidity. Provide a hardcoded list of references upon declaring so that these relationships
		// can be altered at will by the client (maybe they want temperature and pressure).
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				std::shared_ptr<T> device = std::static_pointer_cast<T>( *devicesIt );

				// System settings (settings that start with an underscore and are usually defined by #define)
				// should always overwrite existing system settings.
				for ( auto settingsIt = settings_.begin(); settingsIt != settings_.end(); settingsIt++ ) {
					if ( settingsIt->first.at( 0 ) == '_' ) {
						device->getSettings().put( settingsIt->first, settingsIt->second );
					}
				}
				if ( device->getSettings().isDirty() ) {
					device->getSettings().commit( *device.get() );
				}

				return device;
			}
		}

		long id = g_database->putQuery(
			"INSERT INTO `devices` ( `hardware_id`, `reference`, `type`, `label`, `enabled` ) "
			"VALUES ( %d, %Q, %d, %Q, %d )",
			this->m_id, reference_.c_str(), static_cast<int>( T::type ), label_.c_str(),
			start_ ? 1 : 0
		);
		std::shared_ptr<T> device = std::static_pointer_cast<T>( Device::_factory( this->shared_from_this(), T::type, id, reference_, label_ ) );

		Settings& settings = device->getSettings();
		settings.insert( settings_ );
		if ( settings.isDirty() ) {
			settings.commit( *device );
		}

		if ( start_ ) {
			device->start();
		}
		this->_installDeviceResourceHandlers( device );

		this->m_devices.push_back( device );

		return device;

	};
	template std::shared_ptr<Counter> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_, const bool& start_ );
	template std::shared_ptr<Level> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_, const bool& start_ );
	template std::shared_ptr<Switch> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_, const bool& start_ );
	template std::shared_ptr<Text> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::map<std::string, std::string> settings_, const bool& start_ );

	void Hardware::_installDeviceResourceHandlers( const std::shared_ptr<Device> device_ ) {

		// The first handler to install is the list handler that outputs all the devices available. Note that
		// the output json array is being populated by multiple resource handlers, each one adding another device.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( device_->getId() ),
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( [this,device_]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				if ( output_.is_null() ) {
					output_ = json::array();
				}

				// If a hardware_id property was provided, only devices belonging to that hardware id are being
				// added to the device list.
				auto hardwareIdIt = input_.find( "hardware_id" );
				if (
					hardwareIdIt != input_.end()
					&& input_["hardware_id"].get<std::string>() != std::to_string( this->m_id )
				) {
					return;
				}

				// If the enabled flag was set only devices that are enabled or disabled are returned.
				auto enabledIt = input_.find( "enabled" );
				if ( enabledIt != input_.end() ) {
					bool enabled = true;
					if ( input_["enabled"].is_boolean() ) {
						enabled = input_["enabled"].get<bool>();
					}
					if ( input_["enabled"].is_number() ) {
						enabled = input_["enabled"].get<unsigned int>() > 0;
					}
					if ( enabled && ! device_->isRunning() ) {
						return;
					}
					if ( ! enabled && device_->isRunning() ) {
						return;
					}
				}

				json data = device_->getJson();
				// TODO this causes a lot of requests to the database at once. See if this can be implemented
				// more efficiently.
				data["scripts"] = g_database->getQueryColumn<unsigned int>(
					"SELECT s.`id` "
					"FROM `scripts` s, `x_device_scripts` x "
					"WHERE s.`id`=x.`script_id` "
					"AND x.`device_id`=%d "
					"ORDER BY s.`id` ASC",
					device_->getId()
				);
				output_ += data;
			} )
		} ) ) );

		// The second handler to install is the one that returns the details of a single device and the ability
		// to update or delete the device.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"device-" + std::to_string( device_->getId() ),
			"api/devices/" + std::to_string( device_->getId() ),
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			WebServer::t_callback( [this,device_]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = device_->getJson();
						output_["scripts"] = g_database->getQueryColumn<unsigned int>(
							"SELECT s.`id` "
							"FROM `scripts` s, `x_device_scripts` x "
							"WHERE s.`id`=x.`script_id` "
							"AND x.`device_id`=%d "
							"ORDER BY s.`id` ASC",
							device_->getId()
						);
						break;
					}
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {
						try {
							// A name property can be used to set the custom name for a device.
							if ( input_.find( "name" ) != input_.end() ) {
								device_->getSettings().put( "name", input_["name"].get<std::string>() );
								device_->getSettings().commit( *device_.get() );
							}

							// A scripts array can be passed along to set the scripts to run when the device
							// is updated.
							if (
								input_.find( "scripts") != input_.end()
								&& input_["scripts"].is_array()
							) {
								std::vector<unsigned int> scripts = std::vector<unsigned int>( input_["scripts"].begin(), input_["scripts"].end() );
								device_->setScripts( scripts );
							}

							// The enabled property can be used to enable or disable a device.
							if ( input_.find( "enabled") != input_.end() ) {
								bool enabled = true;
								if ( input_["enabled"].is_boolean() ) {
									enabled = input_["enabled"].get<bool>();
								}
								if ( input_["enabled"].is_number() ) {
									enabled = input_["enabled"].get<unsigned int>() > 0;
								}
								if ( enabled ) {
									if ( ! device_->isRunning() ) {
										device_->start();
									}
								} else {
									if ( device_->isRunning() ) {
										device_->stop();
									}
								}
								g_database->putQuery(
									"UPDATE `devices` "
									"SET `enabled`=%d "
									"WHERE `id`=%d",
									enabled ? 1 : 0,
									device_->getId()
								);

								// Remove all references to this device from the crosstables. A 'delete' will remove these
								// automatically. Disabling means removing them manually.
								if ( ! enabled ) {
									g_database->putQuery(
										"DELETE FROM `x_cron_devices` "
										"WHERE `device_id`=%d",
										device_->getId()
									);
									g_database->putQuery(
										"DELETE FROM `x_device_scripts` "
										"WHERE `device_id`=%d",
										device_->getId()
									);
								}
							}

							// A value property can be set to update the device through the api.
							bool success = true;
							if ( input_.find( "value" ) != input_.end() ) {
								switch( device_->getType() ) {
									case Device::Type::COUNTER:
										if ( input_["value"].is_string() ) {
											success = success && device_->updateValue<Counter>( Device::UpdateSource::API, std::stoi( input_["value"].get<std::string>() ) );
										} else if ( input_["value"].is_number() ) {
											success = success && device_->updateValue<Counter>( Device::UpdateSource::API, input_["value"].get<int>() );
										} else {
											success = false;
											output_["message"] = "Invalid value.";
										}
										break;
									case Device::Type::LEVEL:
										if ( input_["value"].is_string() ) {
											success = success && device_->updateValue<Level>( Device::UpdateSource::API, std::stof( input_["value"].get<std::string>() ) );
										} else if ( input_["value"].is_number() ) {
											success = success && device_->updateValue<Level>( Device::UpdateSource::API, input_["value"].get<double>() );
										} else {
											success = false;
											output_["message"] = "Invalid value.";
										}
										break;
									case Device::Type::SWITCH:
										success = success && device_->updateValue<Switch>( Device::UpdateSource::API, input_["value"].get<std::string>() );
										break;
									case Device::Type::TEXT:
										success = success && device_->updateValue<Text>( Device::UpdateSource::API, input_["value"].get<std::string>() );
										break;
								}
							}
							if ( success ) {
								output_["result"] = "OK";
							} else {
								output_["result"] = "ERROR";
							}
							g_webServer->touchResourceCallback( "device-" + std::to_string( device_->getId() ) );
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to update device.";
							code_ = 400; // bad request
						}
						break;
					}
					case WebServer::Method::DELETE: {
						std::lock_guard<std::mutex> lock( this->m_devicesMutex );
						for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
							if ( (*devicesIt) == device_ ) {

								g_webServer->removeResourceCallback( "device-" + std::to_string( device_->getId() ) );
								if ( device_->isRunning() ) {
									device_->stop();
								}

								g_database->putQuery(
									"DELETE FROM `devices` "
									"WHERE `id`=%d",
									device_->getId()
								);

								this->m_devices.erase( devicesIt );
								g_webServer->touchResourceCallback( "controller" );
								g_webServer->touchResourceCallback( "hardware-" + std::to_string( this->m_id ) );
								output_["result"] = "OK";
								break;
							}
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
	};

	const bool Hardware::_queuePendingUpdate( const std::string& reference_, const unsigned int& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
		// See if there's already a pending update present, in which case we need to use it to start locking.
		std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( reference_ );
		if ( search == this->m_pendingUpdates.end() ) {
			this->m_pendingUpdates[reference_] = std::make_shared<PendingUpdate>( source_ );
		}
		std::shared_ptr<PendingUpdate>& pendingUpdate = this->m_pendingUpdates[reference_];
		pendingUpdatesLock.unlock();

		if ( pendingUpdate->updateMutex.try_lock_for( std::chrono::milliseconds( blockNewUpdate_ ) ) ) {
			std::thread( [this,pendingUpdate,reference_,waitForResult_] {

				std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
				pendingUpdate->condition.wait_for( notifyLock, std::chrono::milliseconds( waitForResult_ ), [&pendingUpdate]{ return pendingUpdate->done; } );
				// Spurious wakeups are someting we have to live with; there's no way to determine if the amount
				// of time was passed or if a spurious wakeup occured.

				std::unique_lock<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
				auto search = this->m_pendingUpdates.find( reference_ );
				if ( search != this->m_pendingUpdates.end() ) {
					this->m_pendingUpdates.erase( search );
				}
				pendingUpdatesLock.unlock();

				pendingUpdate->updateMutex.unlock();
			} ).detach();
			return true;
		} else {
			return false;
		}
	};

	const unsigned int Hardware::_releasePendingUpdate( const std::string& reference_ ) {
		std::lock_guard<std::mutex> pendingUpdatesLock( this->m_pendingUpdatesMutex );
		auto search = this->m_pendingUpdates.find( reference_ );
		if ( search != this->m_pendingUpdates.end() ) {
			auto pendingUpdate = search->second;
			std::unique_lock<std::mutex> notifyLock( pendingUpdate->conditionMutex );
			pendingUpdate->done = true;
			notifyLock.unlock();
			pendingUpdate->condition.notify_all();
			return pendingUpdate->source;
		}
		return 0;
	};

} // namespace micasa
