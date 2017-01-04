// v7
// https://docs.cesanta.com/v7/master/

#include <iostream>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <dirent.h>

#include "Controller.h"
#include "Database.h"
#include "Utils.h"

#ifdef _WITH_LIBUDEV
#include <libudev.h>
#endif // _WITH_LIBUDEV

#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG

v7_err micasa_v7_update_device( struct v7* js_, v7_val_t* res_ ) {
	micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );

	std::shared_ptr<micasa::Device> device = nullptr;

	v7_val_t arg0 = v7_arg( js_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->_getDeviceById( v7_get_int( js_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->_getDeviceByLabel( v7_get_string( js_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->_getDeviceByName( v7_get_string( js_, &arg0, NULL ) );
		}
	}
	if ( device == nullptr ) {
		return v7_throwf( js_, "Error", "Invalid device." );
	}

	v7_val_t arg1 = v7_arg( js_, 1 );

	std::string options = "";
	v7_val_t arg2 = v7_arg( js_, 2 );
	if ( v7_is_string( arg2 ) ) {
		options = v7_get_string( js_, &arg2, NULL );
	}

	if ( v7_is_number( arg1 ) ) {
		double value = v7_get_double( js_, arg1 );
		if ( device->getType() == micasa::Device::Type::COUNTER ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Counter>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::LEVEL ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Level>( device ), value, options ) );
		} else {
			return v7_throwf( js_, "Error", "Invalid parameter for device." );
		}
	} else if ( v7_is_string( arg1 ) ) {
		std::string value = v7_get_string( js_, &arg1, NULL );
		if ( device->getType() == micasa::Device::Type::SWITCH ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Switch>( device ), value, options ) );
		} else if ( device->getType() == micasa::Device::Type::TEXT ) {
			*res_ = v7_mk_boolean( js_, controller->_updateDeviceFromScript( std::static_pointer_cast<micasa::Text>( device ), value, options ) );
		} else {
			return v7_throwf( js_, "Error", "Invalid parameter for device." );
		}
	} else {
		return v7_throwf( js_, "Error", "Invalid parameter for device." );
	}

	return V7_OK;
};

v7_err micasa_v7_get_device( struct v7* js_, v7_val_t* res_ ) {
	micasa::Controller* controller = reinterpret_cast<micasa::Controller*>( v7_get_user_data( js_, v7_get_global( js_ ) ) );

	std::shared_ptr<micasa::Device> device = nullptr;

	v7_val_t arg0 = v7_arg( js_, 0 );
	if ( v7_is_number( arg0 ) ) {
		device = controller->_getDeviceById( v7_get_int( js_, arg0 ) );
	} else if ( v7_is_string( arg0 ) ) {
		device = controller->_getDeviceByLabel( v7_get_string( js_, &arg0, NULL ) );
		if ( device == nullptr ) {
			device = controller->_getDeviceByName( v7_get_string( js_, &arg0, NULL ) );
		}
	}
	if ( device == nullptr ) {
		return v7_throwf( js_, "Error", "Invalid device." );
	}

	v7_err js_error;
	switch( device->getType() ) {
		case micasa::Device::Type::SWITCH:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Switch>( device )->getJson().dump().c_str(), res_ );
			break;
		case micasa::Device::Type::TEXT:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Text>( device )->getJson().dump().c_str(), res_ );
			break;
		case micasa::Device::Type::LEVEL:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Level>( device )->getJson().dump().c_str(), res_ );
			break;
		case micasa::Device::Type::COUNTER:
			js_error = v7_parse_json( js_, std::static_pointer_cast<micasa::Counter>( device )->getJson().dump().c_str(), res_ );
			break;
	}

	if ( V7_OK != js_error ) {
		return v7_throwf( js_, "Error", "Internal error." );
	}

	return V7_OK;
};

namespace micasa {

	extern std::shared_ptr<WebServer> g_webServer;
	extern std::shared_ptr<Database> g_database;
	extern std::shared_ptr<Logger> g_logger;

	using namespace std::chrono;

	template<> void Controller::Task::setValue<Level>( const typename Level::t_value& value_ ) { this->levelValue = value_; };
	template<> void Controller::Task::setValue<Counter>( const typename Counter::t_value& value_ ) { this->counterValue = value_; };
	template<> void Controller::Task::setValue<Switch>( const typename Switch::t_value& value_ ) { this->switchValue = value_; };
	template<> void Controller::Task::setValue<Text>( const typename Text::t_value& value_ ) { this->textValue = value_; };

	Controller::Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be created before global Controller instance." );
		assert( g_database && "Global Database instance should be created before global Controller instance." );
		assert( g_logger && "Global Logger instance should be created before global Controller instance." );
#endif // _DEBUG
		this->m_settings.populate();

		// Initialize the v7 javascript environment.
		this->m_v7_js = v7_create();
		v7_val_t root = v7_get_global( this->m_v7_js );
		v7_set_user_data( this->m_v7_js, root, this );

		v7_val_t userDataObj;
		if ( V7_OK != v7_parse_json( this->m_v7_js, this->m_settings.get( "userdata", "{}" ).c_str(), &userDataObj ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in userdata." );
		}
		if ( V7_OK != v7_parse_json( this->m_v7_js, "{}", &userDataObj ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in default userdata." );
		}
		v7_set( this->m_v7_js, root, "userData", ~0, userDataObj );

		v7_set_method( this->m_v7_js, root, "updateDevice", &micasa_v7_update_device );
		v7_set_method( this->m_v7_js, root, "getDevice", &micasa_v7_get_device );
	};

	Controller::~Controller() {
#ifdef _DEBUG
		assert( g_webServer && "Global WebServer instance should be destroyed after global Controller instance." );
		assert( g_database && "Global Database instance should be destroyed after global Controller instance." );
		assert( g_logger && "Global Logger instance should be destroyed after global Controller instance." );
#endif // _DEBUG

		// Release the v7 javascript environment.
		v7_destroy( this->m_v7_js );
	};

	void Controller::start() {
#ifdef _DEBUG
		assert( g_webServer->isRunning() && "WebServer should be running before Controller is started." );
#endif // _DEBUG
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );

		std::unique_lock<std::mutex> hardwareLock( this->m_hardwareMutex );

		// Fetch all the hardware from the database to initialize our local map of hardware instances.
		std::vector<std::map<std::string, std::string> > hardwareData = g_database->getQuery(
			"SELECT `id`, `hardware_id`, `reference`, `type`, `enabled` "
			"FROM `hardware` "
			"ORDER BY `id` ASC" // child hardware should come *after* parents
		);
		for ( auto hardwareIt = hardwareData.begin(); hardwareIt != hardwareData.end(); hardwareIt++ ) {
			unsigned int parentId = atoi( (*hardwareIt)["hardware_id"].c_str() );
			std::shared_ptr<Hardware> parent = nullptr;
			if ( parentId > 0 ) {
				for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
					if ( (*hardwareIt)->getId() == parentId ) {
						parent = (*hardwareIt);
						break;
					}
				}
			}

			Hardware::Type type = static_cast<Hardware::Type>( atoi( (*hardwareIt)["type"].c_str() ) );
			std::shared_ptr<Hardware> hardware = Hardware::_factory( type, std::stoi( (*hardwareIt)["id"] ), (*hardwareIt)["reference"], parent );

			if ( (*hardwareIt)["enabled"] == "1" ) {
				hardware->start();
			}
			this->_installHardwareResourceHandlers( hardware );

			this->m_hardware.push_back( hardware );
		}
		hardwareLock.unlock();

		// Install the script- and cron resource handlers.
		this->_updateScriptResourceHandlers();
		this->_updateCronResourceHandlers();
		this->_updateUsbResourceHandlers();

		// Add generic resource handlers for the hardware- and device resources.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller",
			"api/hardware",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						if ( output_.is_null() ) {
							output_ = json::array();
						}
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "type" ) != input_.end()
								&& input_.find( "name" ) != input_.end()
							) {

								// Find the hardware type in the declared types from the hardware class.
								int type = -1;
								for ( auto typeIt = Hardware::TypeText.begin(); typeIt != Hardware::TypeText.end(); typeIt++ ) {
									if ( input_["type"].get<std::string>() == typeIt->second ) {
										type = typeIt->first;
									}
								}
								if ( type > -1 ) {
									std::string reference = randomString( 16 );
									// TODO remove the last settings parameter, not needed because hardware isn't started
									// automatically.
									auto hardware = this->declareHardware( static_cast<Hardware::Type>( type ), reference, { } );

									// A settings object can be passed along.
									if (
										input_.find( "settings") != input_.end()
										&& input_["settings"].is_object()
									) {
										std::map<std::string,std::string> settings;
										for ( auto settingsIt = input_["settings"].begin(); settingsIt != input_["settings"].end(); settingsIt++ ) {
											settings[settingsIt.key()] = settingsIt.value();
										}
										hardware->setSettings( settings );
									}

									output_["result"] = "OK";
									output_["hardware"] = hardware->getJson();
									g_webServer->touchResourceCallback( "controller" );
								} else {
									output_["result"] = "ERROR";
									output_["message"] = "Invalid type.";
									code_ = 400; // bad request
								}
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing fields.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to add hardware.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller",
			"api/devices",
			WebServer::Method::GET,
			WebServer::t_callback( []( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				if ( output_.is_null() ) {
					output_ = json::array();
				}
			} )
		} ) ) );

		Worker::start();
		g_logger->log( Logger::LogLevel::NORMAL, this, "Started." );
	};

	void Controller::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );

		g_webServer->removeResourceCallback( "controller" );
		g_webServer->removeResourceCallback( "controller-scripts" );
		g_webServer->removeResourceCallback( "controller-crons" );
		g_webServer->removeResourceCallback( "controller-usb" );

		Worker::stop();

		{
			std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
			for( auto hardwareIt = this->m_hardware.begin(); hardwareIt < this->m_hardware.end(); hardwareIt++ ) {
				auto hardware = (*hardwareIt);
				g_webServer->removeResourceCallback( "hardware-" + std::to_string( hardware->getId() ) );
				if ( hardware->isRunning() ) {
					hardware->stop();
				}
			}
			this->m_hardware.clear();
		}

		g_logger->log( Logger::LogLevel::NORMAL, this, "Stopped." );
	};

	std::shared_ptr<Hardware> Controller::getHardware( const std::string reference_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				return *hardwareIt;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::map<std::string, std::string> settings_, const bool& start_ ) {
		return this->declareHardware( type_, reference_, nullptr, settings_, start_ );
	};

	std::shared_ptr<Hardware> Controller::declareHardware( const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_, const std::map<std::string, std::string> settings_, const bool& start_ ) {
#ifdef _DEBUG
		assert( g_webServer->isRunning() && "WebServer should be running before declaring hardware." );
#endif // _DEBUG

		std::unique_lock<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			if ( (*hardwareIt)->getReference() == reference_ ) {
				return *hardwareIt;
			}
		}

		long id;
		if ( parent_ ) {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `hardware_id`, `reference`, `type`, `enabled` ) "
				"VALUES ( %d, %Q, %d, %d )",
				parent_->getId(), reference_.c_str(), static_cast<int>( type_ ),
				start_ ? 1 : 0
			);
		} else {
			id = g_database->putQuery(
				"INSERT INTO `hardware` ( `reference`, `type`, `enabled` ) "
				"VALUES ( %Q, %d, %d )",
				reference_.c_str(), static_cast<int>( type_ ),
				start_ ? 1 : 0
			);
		}

		// The lock is released while instantiating hardware because some hardware need to lookup their parent with
		// the getHardware* methods which also use the lock.
		lock.unlock();

		std::shared_ptr<Hardware> hardware = Hardware::_factory( type_, id, reference_, parent_ );

		Settings& settings = hardware->getSettings();
		settings.insert( settings_ );
		if ( settings.isDirty() ) {
			settings.commit( *hardware );
		}

		if ( start_ ) {
			hardware->start();
		}
		this->_installHardwareResourceHandlers( hardware );

		// Reacquire lock when adding to the map to make it thread safe.
		lock = std::unique_lock<std::mutex>( this->m_hardwareMutex );
		this->m_hardware.push_back( hardware );
		lock.unlock();

		return hardware;
	};

	template<class D> void Controller::newEvent( const D& device_, const unsigned int& source_ ) {
		// Events originating from scripts should not cause another script run.
		// TODO make this configurable per device?
		if (
			( source_ & Device::UpdateSource::SCRIPT ) != Device::UpdateSource::SCRIPT
			&& ( source_ & Device::UpdateSource::INIT ) != Device::UpdateSource::INIT
		) {
			json event;
			event["value"] = device_.getValue();
			event["source"] = source_;
			event["device"] = device_.getJson();
			event["hardware"] = device_.getHardware()->getJson();

			// The processing of the event is deliberatly done in a separate method because this method is templated
			// and is essentially copied for each specialization.
			// TODO Only fetch those scripts that are present in the script / device crosstable.
			this->_runScripts( "event", event, g_database->getQuery(
				"SELECT s.`id`, s.`name`, s.`code`, s.`runs` "
				"FROM `scripts` s, `x_device_scripts` x "
				"WHERE x.`script_id`=s.`id` "
				"AND x.`device_id`=%d "
				"AND s.`enabled`=1 "
				"ORDER BY `id` ASC",
				device_.getId()
			) );
		}
	};
	template void Controller::newEvent( const Switch& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Level& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Counter& device_, const unsigned int& source_ );
	template void Controller::newEvent( const Text& device_, const unsigned int& source_ );

	const milliseconds Controller::_work( const unsigned long int& iteration_ ) {
		auto now = system_clock::now();

		// See if we need to run the crons (every round minute).
		static unsigned int minuteSinceEpoch = duration_cast<minutes>( now.time_since_epoch() ).count();
		if ( minuteSinceEpoch < duration_cast<minutes>( now.time_since_epoch() ).count() ) {
			this->_runCrons();
		}

		// Investigate the front of the queue for tasks that are due. If the next task is not due we're done due to
		// the fact that _scheduleTask keeps the list sorted on scheduled time.
		std::lock_guard<std::mutex> lock( this->m_taskQueueMutex );
		auto taskQueueIt = this->m_taskQueue.begin();
		while(
			taskQueueIt != this->m_taskQueue.end()
			&& (*taskQueueIt)->scheduled <= now + milliseconds( 5 )
		) {
			std::shared_ptr<Task> task = (*taskQueueIt);

			switch( task->device->getType() ) {
				case Device::Type::COUNTER:
					std::static_pointer_cast<Counter>( task->device )->updateValue( task->source, task->counterValue );
					break;
				case Device::Type::LEVEL:
					std::static_pointer_cast<Level>( task->device )->updateValue( task->source, task->levelValue );
					break;
				case Device::Type::SWITCH:
					std::static_pointer_cast<Switch>( task->device )->updateValue( task->source, task->switchValue );
					break;
				case Device::Type::TEXT:
					std::static_pointer_cast<Text>( task->device )->updateValue( task->source, task->textValue );
					break;
			}

			taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
		}

		// This method should run a least every minute for cron jobs to run. The 5ms is a safe margin to make sure
		// the whole minute has passed.
		auto wait = milliseconds( 60005 ) - duration_cast<milliseconds>( now.time_since_epoch() ) % milliseconds( 60000 );

		// If there's nothing in the queue anymore we can wait the default duration before investigating the queue
		// again. This is because the _scheduleTask method will wake us up if there's work to do anyway.
		if ( taskQueueIt != this->m_taskQueue.end() ) {
			auto delay = duration_cast<milliseconds>( (*taskQueueIt)->scheduled - now );
			if ( delay < wait ) {
				wait = delay;
			}
		}

		return wait;
	};

	void Controller::_runScripts( const std::string& key_, const json& data_, const std::vector<std::map<std::string, std::string> >& scripts_ ) {
		// Event processing is done in a separate thread to prevent scripts from blocking hardare updates.
		// TODO insert a task that checks if the script isn't running for more than xx seconds? This requires that
		// we don't detach and keep track of the thread.
		auto thread = std::thread( [this,key_,data_,scripts_]{
			std::lock_guard<std::mutex> lock( this->m_scriptsMutex );

			v7_val_t root = v7_get_global( this->m_v7_js );

			// Configure the v7 javascript environment with context data.
			v7_val_t dataObj;
			if ( V7_OK != v7_parse_json( this->m_v7_js, data_.dump().c_str(), &dataObj ) ) {
				g_logger->log( Logger::LogLevel::ERROR, this, "Syntax error in scriptdata." );
				return;
			}
			v7_set( this->m_v7_js, root, key_.c_str(), ~0, dataObj );

			for ( auto scriptsIt = scripts_.begin(); scriptsIt != scripts_.end(); scriptsIt++ ) {

				v7_val_t js_result;
				v7_err js_error = v7_exec( this->m_v7_js, (*scriptsIt).at( "code" ).c_str(), &js_result );

				bool success = true;

				switch( js_error ) {
					case V7_SYNTAX_ERROR:
						g_logger->logr( Logger::LogLevel::ERROR, this, "Syntax error in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						success = false;
						break;
					case V7_EXEC_EXCEPTION:
						// Extract error message from result. Note that if the buffer is too small, v7 allocates it's
						// own memory chucnk which we need to free manually.
						char buffer[100], *p;
						p = v7_stringify( this->m_v7_js, js_result, buffer, sizeof( buffer ), V7_STRINGIFY_DEFAULT );
						g_logger->logr( Logger::LogLevel::ERROR, this, "Exception in in \"%s\" (%s).", (*scriptsIt).at( "name" ).c_str(), p );
						if ( p != buffer ) {
							free(p);
						}
						success = false;
						break;
					case V7_AST_TOO_LARGE:
					case V7_INTERNAL_ERROR:
						g_logger->logr( Logger::LogLevel::ERROR, this, "Internal error in in \"%s\".", (*scriptsIt).at( "name" ).c_str() );
						success = false;
						break;
					case V7_OK:
						g_logger->logr( Logger::LogLevel::NORMAL, this, "Script \"%s\" executed.", (*scriptsIt).at( "name" ).c_str() );
						break;
				}

				g_database->putQuery(
					"UPDATE `scripts` "
					"SET `runs`=`runs`+1, `enabled`=%d "
					"WHERE `id`=%q",
					success ? 1 : 0,
					(*scriptsIt).at( "id" ).c_str()
				);
				g_webServer->touchResourceCallback( "controller-scripts" );
			}

			// Get the userdata from the v7 environment and store it in settings for later use. The same logic
			// applies here as V7_EXEC_EXCEPTION regarding the buffer.
			// TODO do we need to do this after *every* script run, or maybe do this periodically.
			v7_val_t userDataObj = v7_get( this->m_v7_js, root, "userData", ~0 );
			char buffer[1024], *p;
			p = v7_stringify( this->m_v7_js, userDataObj, buffer, sizeof( buffer ), V7_STRINGIFY_JSON );
			if ( p != buffer ) {
				free( p );
			}

			this->m_settings.put( "userdata", std::string( buffer ) );
			this->m_settings.commit();
		} );
		thread.detach();
	};

	void Controller::_runCrons() {
		std::lock_guard<std::mutex> lock( this->m_cronsMutex );

		auto crons = g_database->getQuery(
			"SELECT DISTINCT c.`id`, c.`cron`, c.`name` "
			"FROM `crons` c, `x_cron_scripts` x, `scripts` s "
			"WHERE c.`id`=x.`cron_id` "
			"AND s.`id`=x.`script_id` "
			"AND c.`enabled`=1 "
			"AND s.`enabled`=1 "
			"ORDER BY c.`id` ASC"
		);
		for ( auto cronIt = crons.begin(); cronIt != crons.end(); cronIt++ ) {
			try {
				bool run = true;

				// Split the cron string into exactly 5 fields; m h dom mon dow.
				std::vector<std::pair<unsigned int, unsigned int> > extremes = { { 0, 59 }, { 0, 23 }, { 1, 31 }, { 1, 12 }, { 1, 7 } };
				auto fields = stringSplit( (*cronIt)["cron"], ' ' );
				if ( fields.size() != 5 ) {
					throw std::runtime_error( "invalid number of cron fields" );
				}
				for ( unsigned int field = 0; field <= 4; field++ ) {

					// Determine the values that are valid for each field by parsing the field and
					// filling an array with valid values.
					std::vector<std::string> subexpressions;
					if ( fields[field].find( "," ) != std::string::npos ) {
						subexpressions = stringSplit( fields[field], ',' );
					} else {
						subexpressions.push_back( fields[field] );
					}

					std::vector<unsigned int> rangeitems;

					for ( auto& subexpression : subexpressions ) {
						unsigned int start = extremes[field].first;
						unsigned int end = extremes[field].second;
						unsigned int modulo = 0;

						if ( subexpression.find( "/" ) != std::string::npos ) {
							auto parts = stringSplit( subexpression, '/' );
							if ( parts.size() != 2 ) {
								throw std::runtime_error( "invalid cron field devider" );
							}
							modulo = std::stoi( parts[1] );
							subexpression = parts[0];
						}

						if ( subexpression.find( "-" ) != std::string::npos ) {
							auto parts = stringSplit( subexpression, '-' );
							if ( parts.size() != 2 ) {
								throw std::runtime_error( "invalid cron field range" );
							}
							start = std::stoi( parts[0] );
							end = std::stoi( parts[1] );
						} else if (
							subexpression != "*"
							&& modulo == 0
						) {
							start = std::stoi( subexpression );
							end = start;
						}

						for ( unsigned int index = start; index <= end; index++ ) {
							if ( modulo > 0 ) {
								int remainder = index % modulo;
								if ( subexpression == "*" ) {
									if ( 0 == remainder ) {
										rangeitems.push_back( index );
									}
								} else if ( remainder == std::stoi( subexpression ) ) {
									rangeitems.push_back( index );
								}
							} else {
								rangeitems.push_back( index );
							}
						}
					}

					// Then we need to check if the current value for the field is available in the
					// array with valid items.
					time_t rawtime;
					time( &rawtime );

					struct tm* timeinfo;
					timeinfo = localtime( &rawtime );

					unsigned int current;
					switch( field ) {
						case 0: current = timeinfo->tm_min; break;
						case 1: current = timeinfo->tm_hour; break;
						case 2: current = timeinfo->tm_mday; break;
						case 3: current = timeinfo->tm_mon + 1; break;
						case 4: current = timeinfo->tm_wday == 0 ? 7 : timeinfo->tm_wday; break;
					}

					if ( std::find( rangeitems.begin(), rangeitems.end(), current ) == rangeitems.end() ) {
						run = false;
					}

					// If this field didn't match there's no need to investigate the other fields.
					if ( ! run ) {
						break;
					}
				}

				if ( run ) {
					json data = (*cronIt);
					this->_runScripts( "cron", data, g_database->getQuery(
						"SELECT s.`id`, s.`name`, s.`code`, s.`runs` "
						"FROM `x_cron_scripts` x, `scripts` s "
						"WHERE x.`script_id`=s.`id` "
						"AND x.`cron_id`=%q "
						"AND s.`enabled`=1 "
						"ORDER BY s.`id` ASC",
						(*cronIt)["id"].c_str()
					) );
				}

			} catch( std::exception ex_ ) {

				// Something went wrong while parsing the cron. The cron is marked as disabled.
				g_logger->logr( Logger::LogLevel::ERROR, this, "Invalid cron for %s (%s).", (*cronIt).at( "name" ).c_str(), ex_.what() );
				g_database->putQuery(
					"UPDATE `crons` "
					"SET `enabled`=0 "
					"WHERE `id`=%q",
					(*cronIt).at( "id" ).c_str()
				);
			}
		}
	};

	std::shared_ptr<Device> Controller::_getDeviceById( const unsigned int& id_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceById( id_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::_getDeviceByName( const std::string& name_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceByName( name_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	std::shared_ptr<Device> Controller::_getDeviceByLabel( const std::string& label_ ) const {
		std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
		for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
			auto device = (*hardwareIt)->_getDeviceByLabel( label_ );
			if ( device != nullptr ) {
				return device;
			}
		}
		return nullptr;
	};

	template<class D> bool Controller::_updateDeviceFromScript( const std::shared_ptr<D>& device_, const typename D::t_value& value_, const std::string& options_ ) {
		TaskOptions options = this->_parseTaskOptions( options_ );

		if ( options.clear ) {
			this->_clearTaskQueue( device_ );
		}

		typename D::t_value previousValue = device_->getValue();
		for ( int i = 0; i < abs( options.repeat ); i++ ) {
			double delaySec = options.afterSec + ( i * options.forSec ) + ( i * options.repeatSec );

			// TODO implement "recur" task option > if set do not set source to script so that the result of the
			// task will also be executed by scripts. There needs to be some detection though to prevent a loop.
			Task task = { device_, options.recur ? (unsigned int)0 : Device::UpdateSource::SCRIPT, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
			task.setValue<D>( value_ );
			this->_scheduleTask( std::make_shared<Task>( task ) );

			if (
				options.forSec > 0.05
				&& (
					options.repeat > 0
					|| i < abs( options.repeat ) - 1
				)
			) {
				delaySec += options.forSec;
				Task task = { device_, options.recur ? (unsigned int)0 : Device::UpdateSource::SCRIPT, system_clock::now() + milliseconds( (int)( delaySec * 1000 ) ) };
				task.setValue<D>( previousValue );
				this->_scheduleTask( std::make_shared<Task>( task ) );
			}
		}

		return true;
	};
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Level>& device_, const typename Level::t_value& value_, const std::string& options_ );
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Counter>& device_, const typename Counter::t_value& value_, const std::string& options_ );
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Switch>& device_, const typename Switch::t_value& value_, const std::string& options_ );
	template bool Controller::_updateDeviceFromScript( const std::shared_ptr<Text>& device_, const typename Text::t_value& value_, const std::string& options_ );

	void Controller::_clearTaskQueue( const std::shared_ptr<Device>& device_ ) {
		std::unique_lock<std::mutex> lock( this->m_taskQueueMutex );
		for ( auto taskQueueIt = this->m_taskQueue.begin(); taskQueueIt != this->m_taskQueue.end(); ) {
			// TODO check if comparing devices directly also works instead of their ids.
			if ( (*taskQueueIt)->device->getId() == device_->getId() ) {
				taskQueueIt = this->m_taskQueue.erase( taskQueueIt );
			} else {
				taskQueueIt++;
			}
		}
	};

	void Controller::_scheduleTask( const std::shared_ptr<Task> task_ ) {
		std::unique_lock<std::mutex> lock( this->m_taskQueueMutex );

		// The task is inserted in the list in order of scheduled time. This way the worker method only needs to
		// investigate the front of the queue.
		auto taskIt = this->m_taskQueue.begin();
		while(
			  taskIt != this->m_taskQueue.end()
			  && task_->scheduled > (*taskIt)->scheduled
		) {
			taskIt++;
		}
		this->m_taskQueue.insert( taskIt, task_ );
		lock.unlock();

		// Immediately wake up the worker to have it start processing scheduled items.
		this->wakeUp();
	};

	const Controller::TaskOptions Controller::_parseTaskOptions( const std::string& options_ ) const {
		int lastTokenType = 0;
		TaskOptions result = { 0, 0, 1, 0, false, false };

		std::vector<std::string> optionParts;
		stringSplit( options_, ' ', optionParts );
		for ( auto partsIt = optionParts.begin(); partsIt != optionParts.end(); ++partsIt ) {
			std::string part = *partsIt;
			std::transform( part.begin(), part.end(), part.begin(), ::toupper );

			if ( part == "FOR" ) {
				lastTokenType = 1;
			} else if ( part == "AFTER" ) {
				lastTokenType = 2;
			} else if ( part == "REPEAT" ) {
				lastTokenType = 3;
			} else if ( part == "INTERVAL" ) {
				lastTokenType = 4;
			} else if ( part == "CLEAR" ) {
				result.clear = true;
				lastTokenType = 0;
			} else if ( part == "RECUR" ) {
				result.recur = true;
				lastTokenType = 0;
			} else if ( part.find( "SECOND" ) != std::string::npos ) {
				// Do nothing, seconds are the default.
				lastTokenType = 0;
			} else if ( part.find( "MINUTE" ) != std::string::npos ) {
				switch( lastTokenType ) {
					case 1: result.forSec *= 60.; break;
					case 2: result.afterSec *= 60.; break;
					case 4: result.repeatSec *= 60.; break;
				}
				lastTokenType = 0;
			} else if ( part.find( "HOUR" ) != std::string::npos ) {
				switch( lastTokenType ) {
					case 1: result.forSec *= 3600.; break;
					case 2: result.afterSec *= 3600.; break;
					case 4: result.repeatSec *= 3600.; break;
				}
				lastTokenType = 0;
			} else {
				switch( lastTokenType ) {
					case 1:
						result.forSec = std::stod( part );
						break;
					case 2:
						result.afterSec = std::stod( part );
						break;
					case 3:
						result.repeat = std::stoi( part );
						break;
					case 4:
						result.repeatSec = std::stod( part );
						break;
				}
			}
		}

		return result;
	};

	void Controller::_installHardwareResourceHandlers( const std::shared_ptr<Hardware> hardware_ ) {

		// The first handler to install is the list handler that outputs all the hardware available. Note that
		// the output json array is being populated by multiple resource handlers, each one adding another
		// piece of hardware.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( hardware_->getId() ),
			"api/hardware",
			WebServer::Method::GET,
			WebServer::t_callback( [this,hardware_]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				if ( output_.is_null() ) {
					output_ = json::array();
				}

				// If a hardware_id property was provided, only devices belonging to that hardware id are being
				// added to the device list.
				auto hardwareIdIt = input_.find( "hardware_id" );
				if (
					hardwareIdIt != input_.end()
					&& (
						! hardware_->getParent()
						|| input_["hardware_id"].get<std::string>() != std::to_string( hardware_->getParent()->getId() )
					)
				) {
					return;
				} else if (
					hardwareIdIt == input_.end()
					&& hardware_->getParent()
				) {
					return;
				}

				// If the enabled flag was set only hardware that are enabled or disabled are returned.
				auto enabledIt = input_.find( "enabled" );
				if ( enabledIt != input_.end() ) {
					bool enabled = true;
					if ( input_["enabled"].is_boolean() ) {
						enabled = input_["enabled"].get<bool>();
					}
					if ( input_["enabled"].is_number() ) {
						enabled = input_["enabled"].get<unsigned int>() > 0;
					}
					if ( enabled && ! hardware_->isRunning() ) {
						return;
					}
					if ( ! enabled && hardware_->isRunning() ) {
						return;
					}
				}

				output_ += hardware_->getJson();
			} )
		} ) ) );

		// The second handler to install is the one that returns the details of a single piece of hardware and
		// the ability to update or delete the hardware.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"hardware-" + std::to_string( hardware_->getId() ),
			"api/hardware/" + std::to_string( hardware_->getId() ),
			WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::PATCH | WebServer::Method::DELETE,
			WebServer::t_callback( [this,hardware_]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = hardware_->getJson();
						break;
					}
					case WebServer::Method::PUT:
					case WebServer::Method::PATCH: {
						try {
							// A name property can be used to set the custom name for the hardware.
							if ( input_.find( "name" ) != input_.end() ) {
								hardware_->getSettings().put( "name", input_["name"].get<std::string>() );
								hardware_->getSettings().commit( *hardware_.get() );
							}

							// The enabled property can be used to enable or disable the hardware.
							bool enabled = hardware_->isRunning();
							if ( input_.find( "enabled") != input_.end() ) {
								if ( input_["enabled"].is_boolean() ) {
									enabled = input_["enabled"].get<bool>();
								}
								if ( input_["enabled"].is_number() ) {
									enabled = input_["enabled"].get<unsigned int>() > 0;
								}
								g_database->putQuery(
									"UPDATE `hardware` "
									"SET `enabled`=%d "
									"WHERE `id`=%d "
									"OR `hardware_id`=%d",
									enabled ? 1 : 0,
									hardware_->getId(),
									hardware_->getId()
								);
							}

							// A settings object can be passed along.
							if (
								input_.find( "settings") != input_.end()
								&& input_["settings"].is_object()
							) {
								std::map<std::string,std::string> settings;
								for ( auto settingsIt = input_["settings"].begin(); settingsIt != input_["settings"].end(); settingsIt++ ) {
									settings[settingsIt.key()] = settingsIt.value();
								}
								if ( hardware_->isRunning() ) {
									hardware_->stop();
								}
								hardware_->setSettings( settings );
							}

							// Start or stop hardware. All children of the hardware are also started or stopped.
							{
								std::lock_guard<std::mutex> lock( this->m_hardwareMutex );
								if ( enabled ) {
									if ( ! hardware_->isRunning() ) {
										hardware_->start();
									}
									for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware_
											&& ! (*hardwareIt)->isRunning()
										) {
											(*hardwareIt)->start();
										}
									}
								} else {
									if ( hardware_->isRunning() ) {
										hardware_->stop();
									}
									for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); hardwareIt++ ) {
										if (
											(*hardwareIt)->getParent() == hardware_
											&& (*hardwareIt)->isRunning()
										) {
											(*hardwareIt)->stop();
										}
									}
								}
							}

							output_["result"] = "OK";
							g_webServer->touchResourceCallback( "hardware-" + std::to_string( hardware_->getId() ) );
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to update hardware.";
							code_ = 400; // bad request
						}
						break;
					}
					case WebServer::Method::DELETE: {
						std::lock_guard<std::mutex> lock( this->m_hardwareMutex );

						// First all the childs are stopped and removed from the system. The database record is not yet
						// removed because that is done when the parent is removed by foreign key constraints.
						for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); ) {
							if ( (*hardwareIt)->getParent() == hardware_ ) {
								g_webServer->removeResourceCallback( "hardware-" + std::to_string( (*hardwareIt)->getId() ) );
								if ( (*hardwareIt)->isRunning() ) {
									(*hardwareIt)->stop();
								}

								hardwareIt = this->m_hardware.erase( hardwareIt );
								g_webServer->touchResourceCallback( "controller" );
							} else {
								hardwareIt++;
							}
						}
						for ( auto hardwareIt = this->m_hardware.begin(); hardwareIt != this->m_hardware.end(); ) {
							if ( (*hardwareIt) == hardware_ ) {
								g_webServer->removeResourceCallback( "hardware-" + std::to_string( hardware_->getId() ) );
								if ( hardware_->isRunning() ) {
									hardware_->stop();
								}

								g_database->putQuery(
									"DELETE FROM `hardware` "
									"WHERE `id`=%d",
									hardware_->getId()
								);

								this->m_hardware.erase( hardwareIt );
								g_webServer->touchResourceCallback( "controller" );
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

	void Controller::_updateScriptResourceHandlers() const {

		// First all current callbacks for scripts are removed.
		g_webServer->removeResourceCallback( "controller-scripts" );

		// The first callback is the general script fetch callback that lists all available scripts and can
		// be used to add (post) new scripts.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-scripts",
			"api/scripts",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_scriptsMutex );
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = g_database->getQuery<json>(
							"SELECT `id`, `name`, `code`, `enabled`, `runs` "
							"FROM `scripts` "
							"ORDER BY `id` ASC"
						);
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "name") != input_.end()
								&& input_.find( "code") != input_.end()
							) {
								auto scriptId = g_database->putQuery(
									"INSERT INTO `scripts` (`name`, `code`) "
									"VALUES (%Q, %Q) ",
									input_["name"].get<std::string>().c_str(),
									input_["code"].get<std::string>().c_str()
								);
								output_["result"] = "OK";
								output_["script"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `code`, `enabled`, `runs` "
									"FROM `scripts` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									scriptId
								);
								this->_updateScriptResourceHandlers();
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing parameters.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to save script.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );

		// Then a specific resource handler is installed for each script. This handler allows for retrieval as
		// well as updating a script.
		auto scriptIds = g_database->getQueryColumn<unsigned int>(
			"SELECT `id` "
			"FROM `scripts` "
			"ORDER BY `id` ASC"
		);
		for ( auto scriptIdsIt = scriptIds.begin(); scriptIdsIt != scriptIds.end(); scriptIdsIt++ ) {
			unsigned int scriptId = (*scriptIdsIt);
			g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
				"controller-scripts",
				"api/scripts/" + std::to_string( scriptId ),
				WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
				WebServer::t_callback( [this,scriptId]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
					std::lock_guard<std::mutex> lock( this->m_scriptsMutex );
					switch( method_ ) {
						case WebServer::Method::PUT: {
							try {
								if (
									input_.find( "name") != input_.end()
									&& input_.find( "code") != input_.end()
								) {
									g_database->putQuery(
										"UPDATE `scripts` "
										"SET `name`=%Q, `code`=%Q, `runs`=0 "
										"WHERE `id`=%d",
										input_["name"].get<std::string>().c_str(),
										input_["code"].get<std::string>().c_str(),
										scriptId
									);
								}

								// The enabled property can be used to enable or disable the script.
								if ( input_.find( "enabled") != input_.end() ) {
									bool enabled = true;
									if ( input_["enabled"].is_boolean() ) {
										enabled = input_["enabled"].get<bool>();
									}
									if ( input_["enabled"].is_number() ) {
										enabled = input_["enabled"].get<unsigned int>() > 0;
									}
									g_database->putQuery(
										"UPDATE `scripts` "
										"SET `enabled`=%d "
										"WHERE `id`=%d ",
										enabled ? 1 : 0,
										scriptId
									);
								}

								output_["result"] = "OK";
								output_["script"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `code`, `enabled`, `runs` "
									"FROM `scripts` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									scriptId
								);
								g_webServer->touchResourceCallback( "controller-scripts" );

							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save script.";
								code_ = 400; // bad request
							}
							break;
						}
						case WebServer::Method::GET: {
							output_ = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `code`, `enabled`, `runs` "
								"FROM `scripts` "
								"WHERE `id`=%d "
								"ORDER BY `id` ASC",
								scriptId
							);
							break;
						}
						case WebServer::Method::DELETE: {
							try {
								g_database->putQuery(
									"DELETE FROM `scripts` "
									"WHERE `id`=%d",
									scriptId
								);
								this->_updateScriptResourceHandlers();
								output_["result"] = "OK";
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to delete script.";
								code_ = 400; // bad request
							}
							break;
						}
						default: break;
					}
				} )
			} ) ) );
		}
	};

	void Controller::_updateCronResourceHandlers() const {

		// First all current callbacks for crons are removed.
		g_webServer->removeResourceCallback( "controller-crons" );

		const auto setScripts = []( const json& scriptIds_, unsigned int cronId_ ) {
			std::stringstream list;
			unsigned int index = 0;
			for ( auto scriptIdsIt = scriptIds_.begin(); scriptIdsIt != scriptIds_.end(); scriptIdsIt++ ) {
				auto scriptId = (*scriptIdsIt);
				if ( scriptId.is_number() ) {
					list << ( index > 0 ? "," : "" ) << scriptId;
					index++;
					g_database->putQuery(
						"INSERT OR IGNORE INTO `x_cron_scripts` "
						"(`cron_id`, `script_id`) "
						"VALUES (%d, %d)",
						cronId_,
						scriptId.get<unsigned int>()
					);
				}
			}
			g_database->putQuery(
				"DELETE FROM `x_cron_scripts` "
				"WHERE `cron_id`=%d "
				"AND `script_id` NOT IN (%q)",
				cronId_,
				list.str().c_str()
			);
		};

		// The first callback is the general cron fetch callback that lists all available crons and can
		// be used to add (post) new crons.
		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-crons",
			"api/crons",
			WebServer::Method::GET | WebServer::Method::POST,
			WebServer::t_callback( [this,&setScripts]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
				std::lock_guard<std::mutex> lock( this->m_cronsMutex );
				switch( method_ ) {
					case WebServer::Method::GET: {
						output_ = json::array();
						auto crons = g_database->getQuery<json>(
							"SELECT `id`, `name`, `cron`, `enabled` "
							"FROM `crons` "
							"ORDER BY `id` ASC"
						);
						for ( auto cronsIt = crons.begin(); cronsIt != crons.end(); cronsIt++ ) {
							json cron = (*cronsIt);
							cron["scripts"] = g_database->getQueryColumn<unsigned int>(
								"SELECT s.`id` "
								"FROM `scripts` s, `x_cron_scripts` x "
								"WHERE s.`id`=x.`script_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY s.`id` ASC",
								(*cronsIt)["id"].get<unsigned int>()
							);
							cron["devices"] = g_database->getQuery<json>(
								"SELECT d.`id`, x.`value` "
								"FROM `devices` d, `x_cron_devices` x "
								"WHERE d.`id`=x.`device_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY d.`id` ASC",
								(*cronsIt)["id"].get<unsigned int>()
							);
							output_ += cron;
						}
						break;
					}
					case WebServer::Method::POST: {
						try {
							if (
								input_.find( "name") != input_.end()
								&& input_.find( "cron") != input_.end()
							) {
								auto cronId = g_database->putQuery(
									"INSERT INTO `crons` (`name`, `cron`) "
									"VALUES (%Q, %Q) ",
									input_["name"].get<std::string>().c_str(),
									input_["cron"].get<std::string>().c_str()
								);
								if (
									input_.find( "scripts") != input_.end()
									&& input_["scripts"].is_array()
								) {
									setScripts( input_["scripts"], cronId );
								}
								output_["result"] = "OK";
								output_["cron"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `cron` "
									"FROM `crons` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									cronId
								);
								this->_updateCronResourceHandlers();
							} else {
								output_["result"] = "ERROR";
								output_["message"] = "Missing parameters.";
								code_ = 400; // bad request
							}
						} catch( ... ) {
							output_["result"] = "ERROR";
							output_["message"] = "Unable to save cron.";
							code_ = 400; // bad request
						}
						break;
					}
					default: break;
				}
			} )
		} ) ) );

		// Then a specific resource handler is installed for each cron. This handler allows for retrieval as
		// well as updating a cron.
		auto cronIds = g_database->getQueryColumn<unsigned int>(
			"SELECT `id` "
			"FROM `crons` "
			"ORDER BY `id` ASC"
		);
		for ( auto cronIdsIt = cronIds.begin(); cronIdsIt != cronIds.end(); cronIdsIt++ ) {
			unsigned int cronId = (*cronIdsIt);
			g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
				"controller-crons",
				"api/crons/" + std::to_string( cronId ),
				WebServer::Method::GET | WebServer::Method::PUT | WebServer::Method::DELETE,
				WebServer::t_callback( [this,cronId,&setScripts]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {
					std::lock_guard<std::mutex> lock( this->m_cronsMutex );
					switch( method_ ) {
						case WebServer::Method::PUT: {
							try {
								if (
									input_.find( "name") != input_.end()
									&& input_.find( "cron") != input_.end()
								) {
									g_database->putQuery(
										"UPDATE `crons` "
										"SET `name`=%Q, `cron`=%Q "
										"WHERE `id`=%d",
										input_["name"].get<std::string>().c_str(),
										input_["cron"].get<std::string>().c_str(),
										cronId
									);
								}

								if (
									input_.find( "scripts") != input_.end()
									&& input_["scripts"].is_array()
								) {
									setScripts( input_["scripts"], cronId );
								}

								// The enabled property can be used to enable or disable the script.
								if ( input_.find( "enabled") != input_.end() ) {
									bool enabled = true;
									if ( input_["enabled"].is_boolean() ) {
										enabled = input_["enabled"].get<bool>();
									}
									if ( input_["enabled"].is_number() ) {
										enabled = input_["enabled"].get<unsigned int>() > 0;
									}
									g_database->putQuery(
										"UPDATE `crons` "
										"SET `enabled`=%d "
										"WHERE `id`=%d ",
										enabled ? 1 : 0,
										cronId
									);
								}

								output_["result"] = "OK";
								output_["cron"] = g_database->getQueryRow<json>(
									"SELECT `id`, `name`, `cron`, `enabled` "
									"FROM `crons` "
									"WHERE `id`=%d "
									"ORDER BY `id` ASC",
									cronId
								);
								g_webServer->touchResourceCallback( "controller-crons" );

							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to save cron.";
								code_ = 400; // bad request
							}
							break;
						}
						case WebServer::Method::GET: {
							output_ = g_database->getQueryRow<json>(
								"SELECT `id`, `name`, `cron`, `enabled` "
								"FROM `crons` "
								"WHERE `id`=%d "
								"ORDER BY `id` ASC",
								cronId
							);
							output_["scripts"] = g_database->getQueryColumn<unsigned int>(
								"SELECT s.`id` "
								"FROM `scripts` s, `x_cron_scripts` x "
								"WHERE s.`id`=x.`script_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY s.`id` ASC",
								cronId
							);
							output_["devices"] = g_database->getQuery<json>(
								"SELECT d.`id`, x.`value` "
								"FROM `devices` d, `x_cron_devices` x "
								"WHERE d.`id`=x.`device_id` "
								"AND x.`cron_id`=%d "
								"ORDER BY d.`id` ASC",
								cronId
							);
							break;
						}
						case WebServer::Method::DELETE: {
							try {
								g_database->putQuery(
									"DELETE FROM `crons` "
									"WHERE `id`=%d",
									cronId
								);
								this->_updateCronResourceHandlers();
								output_["result"] = "OK";
							} catch( ... ) {
								output_["result"] = "ERROR";
								output_["message"] = "Unable to delete cron.";
								code_ = 400; // bad request
							}
							break;
						}
						default: break;
					}
				} )
			} ) ) );
		}
	};

	void Controller::_updateUsbResourceHandlers() const {

		// http://stackoverflow.com/questions/2530096/how-to-find-all-serial-devices-ttys-ttyusb-on-linux-without-opening-them
		// http://www.signal11.us/oss/udev/
		g_webServer->removeResourceCallback( "controller-usb" );

		g_webServer->addResourceCallback( std::make_shared<WebServer::ResourceCallback>( WebServer::ResourceCallback( {
			"controller-usb",
			"api/usb",
			WebServer::Method::GET,
			WebServer::t_callback( [this]( const std::string& uri_, const json& input_, const WebServer::Method& method_, int& code_, json& output_ ) {

				output_ = json::array();

				// TODO find other means of obtaining the list of usb devices if libudev is not available.
				
#ifdef _WITH_LIBUDEV
				// Create the udef object which is used to discover tty usb devices.
				udev* udev = udev_new();
				if ( ! udev ) {
					g_logger->log( Logger::LogLevel::ERROR, this, "Unable to create udev." );
					output_["result"] = "ERROR";
					output_["message"] = "Unable to create udef.";
					return;
				}

				// Iterate through the list of devices in the tty subsystem.
				udev_enumerate* enumerate = udev_enumerate_new( udev );
				udev_enumerate_add_match_is_initialized( enumerate );
				udev_enumerate_add_match_subsystem( enumerate, "tty" );
				udev_enumerate_scan_devices( enumerate );

				udev_list_entry* devices = udev_enumerate_get_list_entry( enumerate );
				udev_list_entry* dev_list_entry;
				udev_list_entry_foreach( dev_list_entry, devices ) {

					// Extract the full path to the tty device and create a udev device out of it.
					const char* path = udev_list_entry_get_name( dev_list_entry );
					udev_device* dev = udev_device_new_from_syspath( udev, path );

					// Get the device node and see if it's valid. If so, it can be used as an usb device
					// for various hardware. The parent usb device holds information on the device.
					const char* node = udev_device_get_devnode( dev );
					if ( node ) {
						struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype( dev, "usb", "usb_device" );
						if ( parent ) {
							output_ += {
								{ "path", node },
								{ "id_vendor", udev_device_get_sysattr_value( parent, "idVendor" ) ?: "" },
								{ "id_product", udev_device_get_sysattr_value( parent, "idProduct" ) ?: "" },
								{ "manufacturer", udev_device_get_sysattr_value( parent, "manufacturer" ) ?: "" },
								{ "product", udev_device_get_sysattr_value( parent, "product" ) ?: "" },
								{ "serial", udev_device_get_sysattr_value( parent, "serial" ) ?: "" }
							};
						}
					}

					udev_device_unref( dev );
				}

				// Free the enumerator object.
				udev_enumerate_unref( enumerate );
				udev_unref( udev );
#endif // _WITH_LIBUDEV

			} )
		} ) ) );
	};

}; // namespace micasa
