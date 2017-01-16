#include <iostream>
#include <sstream>

#include "Hardware.h"
#include "Database.h"
#include "Controller.h"
#include "Utils.h"
#include "User.h"

#ifdef _WITH_OPENZWAVE
	#include "hardware/ZWave.h"
	#include "hardware/ZWaveNode.h"
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

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {

	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Controller> g_controller;
	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Logger> g_logger;

	const std::map<Hardware::Type, std::string> Hardware::TypeText = {
		{ Hardware::Type::HARMONY_HUB, "harmony_hub" },
#ifdef _WITH_OPENZWAVE
		{ Hardware::Type::ZWAVE, "zwave" },
		{ Hardware::Type::ZWAVE_NODE, "zwave_node" },
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
		{ Hardware::State::SLEEPING, "sleeping" },
		{ Hardware::State::DISCONNECTED, "disconnected" }
	};

	Hardware::Hardware( const unsigned int id_, const Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Worker(), m_id( id_ ), m_type( type_ ), m_reference( reference_ ), m_parent( parent_ ) {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before Hardware instances." );
		assert( g_database && "Global Database instance should be created before Hardware instances." );
		assert( g_logger && "Global Logger instance should be created before Hardware instances." );
#endif // _DEBUG
		this->m_settings = std::make_shared<Settings<Hardware> >( *this );
	};

	Hardware::~Hardware() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after Hardware instances." );
		assert( g_database && "Global Database instance should be destroyed after Hardware instances." );
		assert( g_logger && "Global Logger instance should be destroyed after Hardware instances." );
#endif // _DEBUG
	};

	std::shared_ptr<Hardware> Hardware::factory( const Type type_, const unsigned int id_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) {
		switch( type_ ) {
			case HARMONY_HUB:
				return std::make_shared<HarmonyHub>( id_, type_, reference_, parent_ );
				break;
#ifdef _WITH_OPENZWAVE
			case ZWAVE:
				return std::make_shared<ZWave>( id_, type_, reference_, parent_ );
				break;
			case ZWAVE_NODE:
				return std::make_shared<ZWaveNode>( id_, type_, reference_, parent_ );
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
			std::shared_ptr<Device> device = Device::factory( this->shared_from_this(), type, std::stoi( (*devicesIt)["id"] ), (*devicesIt)["reference"], (*devicesIt)["label"] );

			if ( (*devicesIt)["enabled"] == "1" ) {
				device->start();
			}
			this->_installDeviceResourceHandlers( device );

			this->m_devices.push_back( device );
		}

		// Set the state to initializing if the hardware itself didn't already set the state to something else.
		if ( this->getState() == DISABLED ) {
			this->setState( INIT );
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

		if ( this->m_settings->isDirty() ) {
			this->m_settings->commit();
		}

		this->setState( DISABLED );
		Worker::stop();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	Hardware::Type Hardware::getType() const {
		return this->m_type;
	};
	template<> unsigned int Hardware::getType() const {
		return (unsigned int)this->m_type;
	};
	template<> std::string Hardware::getType() const {
		return Hardware::TypeText.at( this->getType() );
	};

	Hardware::State Hardware::getState() const {
		return this->m_state;
	};
	template<> unsigned int Hardware::getState() const {
		return (unsigned int)this->m_state;
	};
	template<> std::string Hardware::getState() const {
		return Hardware::StateText.at( this->getState() );
	};

	std::string Hardware::getName() const {
		return this->m_settings->get( "name", this->getLabel() );
	};

	void Hardware::setState( const State& state_ ) {
		if ( this->m_state != state_ ) {
			this->m_state = state_;
		}
	};

	json Hardware::getJson( bool full_ ) const {
		json result = {
			{ "id", this->m_id },
			{ "label", this->getLabel() },
			{ "name", this->getName() },
			{ "enabled", this->isRunning() },
			{ "type", this->getType<std::string>() },
			{ "state", this->getState<std::string>() }
		};
		if ( this->m_parent ) {
			result["parent"] = this->m_parent->getJson( full_ );
		}
		if ( full_ ) {
			result["settings"] = json::array();
		}
		
		return result;
	};

	std::shared_ptr<Device> Hardware::getDevice( const std::string& reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getReference() == reference_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getId() == id_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getName() == name_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Hardware::getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_devicesMutex );
		for ( auto devicesIt = this->m_devices.begin(); devicesIt != this->m_devices.end(); devicesIt++ ) {
			if ( (*devicesIt)->getLabel() == label_ ) {
				return *devicesIt;
			}
		}
		return nullptr;
	};

	template<class T> std::shared_ptr<T> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ ) {
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
					if ( (*settingsIt).first.at( 0 ) == '_' ) {
						device->getSettings()->put( settingsIt->first, settingsIt->second );
					}
				}
				if ( device->getSettings()->isDirty() ) {
					device->getSettings()->commit();
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
		std::shared_ptr<T> device = std::static_pointer_cast<T>( Device::factory( this->shared_from_this(), T::type, id, reference_, label_ ) );

		auto settings = device->getSettings();
		settings->insert( settings_ );
		if ( settings->isDirty() ) {
			settings->commit();
		}

		if ( start_ ) {
			device->start();
		}
		this->_installDeviceResourceHandlers( device );

		this->m_devices.push_back( device );

		return device;

	};
	template std::shared_ptr<Counter> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );
	template std::shared_ptr<Level> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );
	template std::shared_ptr<Switch> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );
	template std::shared_ptr<Text> Hardware::_declareDevice( const std::string reference_, const std::string label_, const std::vector<Setting>& settings_, const bool& start_ );

	// TODO do not include settings / hardware etc when fetching the entire list > already taks a few seconds to
	// build, needs to be optional. Maybe only include details in separate get requests for individual devices?
	void Hardware::_installDeviceResourceHandlers( const std::shared_ptr<Device> device_ ) {

		// The first handler to install is the list handler that outputs all the devices available. Note that
		// the output json array is being populated by multiple resource handlers, each one adding another device.
		g_webServer->addResourceCallback( {
			"device-" + std::to_string( device_->getId() ),
			"api/devices",
			100,
			User::Rights::VIEWER,
			WebServer::Method::GET,
			WebServer::t_callback( [this,device_]( const json& input_, const WebServer::Method& method_, json& output_ ) {

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

				output_["data"] += device_->getJson( false );
			} )
		} );

		// The second handler to install is the one that returns the details of a single device and the ability
		// to update or delete the device.
		g_webServer->addResourceCallback( {
			"device-" + std::to_string( device_->getId() ),
			"api/devices/" + std::to_string( device_->getId() ),
			100,
			User::Rights::VIEWER,
			WebServer::Method::GET,
			WebServer::t_callback( [this,device_]( const json& input_, const WebServer::Method& method_, json& output_ ) {
				output_["data"] = device_->getJson( true );
				output_["data"]["scripts"] = g_database->getQueryColumn<unsigned int>(
					"SELECT s.`id` "
					"FROM `scripts` s, `x_device_scripts` x "
					"WHERE s.`id`=x.`script_id` "
					"AND x.`device_id`=%d "
					"ORDER BY s.`id` ASC",
					device_->getId()
				);
				output_["code"] = 200;
			} )
		} );
		
		g_webServer->addResourceCallback( {
			"device-" + std::to_string( device_->getId() ),
			"api/devices/" + std::to_string( device_->getId() ),
			100,
			User::Rights::USER,
			WebServer::Method::PUT | WebServer::Method::PATCH,
			WebServer::t_callback( [this,device_]( const json& input_, const WebServer::Method& method_, json& output_ ) {
				if ( input_.find( "value" ) != input_.end() ) {
					switch( device_->getType() ) {
						case Device::Type::COUNTER:
							if ( input_["value"].is_string() ) {
								device_->updateValue<Counter>( Device::UpdateSource::API, std::stoi( input_["value"].get<std::string>() ) );
							} else if ( input_["value"].is_number() ) {
								device_->updateValue<Counter>( Device::UpdateSource::API, input_["value"].get<int>() );
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Value", "The supplied value is invalid." } );
							}
							break;
						case Device::Type::LEVEL:
							if ( input_["value"].is_string() ) {
								device_->updateValue<Level>( Device::UpdateSource::API, std::stof( input_["value"].get<std::string>() ) );
							} else if ( input_["value"].is_number() ) {
								device_->updateValue<Level>( Device::UpdateSource::API, input_["value"].get<double>() );
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Value", "The supplied value is invalid." } );
							}
							break;
						case Device::Type::SWITCH:
							if ( input_["value"].is_string() ) {
								device_->updateValue<Switch>( Device::UpdateSource::API, input_["value"].get<std::string>() );
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Value", "The supplied value is invalid." } );
							}
							break;
						case Device::Type::TEXT:
							if ( input_["value"].is_string() ) {
								device_->updateValue<Text>( Device::UpdateSource::API, input_["value"].get<std::string>() );
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Value", "The supplied value is invalid." } );
							}
							break;
					}
				}

				output_["data"] = device_->getJson( true );
				output_["data"]["scripts"] = g_database->getQueryColumn<unsigned int>(
					"SELECT s.`id` "
					"FROM `scripts` s, `x_device_scripts` x "
					"WHERE s.`id`=x.`script_id` "
					"AND x.`device_id`=%d "
					"ORDER BY s.`id` ASC",
					device_->getId()
				);
				output_["code"] = 200;
			} )
		} );

		g_webServer->addResourceCallback( {
			"device-" + std::to_string( device_->getId() ),
			"api/devices/" + std::to_string( device_->getId() ),
			100,
			User::Rights::INSTALLER,
			WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			WebServer::t_callback( [this,device_]( const json& input_, const WebServer::Method& method_, json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {

						// A name property can be used to set the custom name for a device.
						if ( input_.find( "name" ) != input_.end() ) {
							if ( input_["name"].is_string() ) {
								device_->getSettings()->put( "name", input_["name"].get<std::string>() );
								device_->getSettings()->commit();
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Name", "The supplied name is invalid." } );
							}
						}

						auto settings = extractSettingsFromJson( input_ );
						if ( settings.find( "unit" ) != settings.end() ) {
							unsigned int unit = std::stoi( settings.at( "unit" ) );
							switch( device_->getType() ) {
								case Device::Type::COUNTER: {
									if ( Counter::UnitText.find( (Counter::Unit)unit ) != Counter::UnitText.end() ) {
										std::static_pointer_cast<Counter>( device_ )->setUnit( (Counter::Unit)unit );
									} else {
										throw WebServer::ResourceException( { 400, "Device.Invalid.Unit", "The supplied unit is invalid." } );
									}
									break;
								}
								case Device::Type::LEVEL: {
									if ( Level::UnitText.find( (Level::Unit)unit ) != Level::UnitText.end() ) {
										std::static_pointer_cast<Level>( device_ )->setUnit( (Level::Unit)unit );
									} else {
										throw WebServer::ResourceException( { 400, "Device.Invalid.Unit", "The supplied unit is invalid." } );
									}
									break;
								}
								default: break;
							}
						}

						// A scripts array can be passed along to set the scripts to run when the device
						// is updated.
						if ( input_.find( "scripts") != input_.end() ) {
							if ( input_["scripts"].is_array() ) {
								std::vector<unsigned int> scripts = std::vector<unsigned int>( input_["scripts"].begin(), input_["scripts"].end() );
								device_->setScripts( scripts );
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Scripts", "The supplied scripts parameter is invalid." } );
							}
						}

						if ( input_.find( "enabled") != input_.end() ) {
							bool enabled = true;
							if ( input_["enabled"].is_boolean() ) {
								enabled = input_["enabled"].get<bool>();
							} else if ( input_["enabled"].is_number() ) {
								enabled = input_["enabled"].get<unsigned int>() > 0;
							} else if ( input_["enabled"].is_string() ) {
								enabled = ( input_["enabled"].get<std::string>() == "1" || input_["enabled"].get<std::string>() == "true" || input_["enabled"].get<std::string>() == "yes" );
							} else {
								throw WebServer::ResourceException( { 400, "Device.Invalid.Enabled", "The supplied enabled parameter is invalid." } );
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
						}

						output_["code"] = 200;
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
								output_["code"] = 200;
								break;
							}
						}
						break;
					}
					default: break;
				}
			} )
		} );
	};

	bool Hardware::_queuePendingUpdate( const std::string& reference_, const unsigned int& source_, const unsigned int& blockNewUpdate_, const unsigned int& waitForResult_ ) {
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

	unsigned int Hardware::_releasePendingUpdate( const std::string& reference_ ) {
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
