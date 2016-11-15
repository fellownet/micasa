#pragma once

namespace micasa {

	const std::vector<std::string> c_queries = {
		"CREATE TABLE IF NOT EXISTS `hardware` ( "
		"`id` INTEGER PRIMARY KEY, " // functions as sqlite3 _rowid_ when named *exactly* INTEGER PRIMARY KEY
		"`unit` VARCHAR(64) NOT NULL, "
		"`type` INTEGER NOT NULL, "
		"`name` VARCHAR(255) NOT NULL, "
		"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_hardware_unit` ON `hardware`( `unit` )",

		"CREATE TABLE IF NOT EXISTS `hardware_settings` ( "
		"`hardware_id` INTEGER NOT NULL, "
		"`key` VARCHAR(64) NOT NULL, "
		"`value` TEXT NOT NULL, "
		"`last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"PRIMARY KEY (`hardware_id`, `key`) "
		"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE CASCADE )",

		"CREATE TABLE IF NOT EXISTS `devices` ( "
		"`id` INTEGER PRIMARY KEY, "
		"`hardware_id` INTEGER NOT NULL, "
		"`unit` VARCHAR(64) NOT NULL, "
		"`type` INTEGER NOT NULL, "
		"`name` VARCHAR(255) NOT NULL, "
		"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE CASCADE )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_devices_unit` ON `devices`( `hardware_id`, `unit` )",

		"CREATE TABLE IF NOT EXISTS `device_settings` ( "
		"`device_id` INTEGER NOT NULL, "
		"`key` VARCHAR(64) NOT NULL, "
		"`value` TEXT NOT NULL, "
		"`last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"PRIMARY KEY (`device_id`, `key`) "
		"FOREIGN KEY ( `device_id` ) REFERENCES `device` ( `id` ) ON DELETE CASCADE ON UPDATE CASCADE )",
	};
	
}; // namespace micasa
