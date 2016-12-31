#pragma once

namespace micasa {

	const std::vector<std::string> c_queries = {
		
		// Hardware
		"CREATE TABLE IF NOT EXISTS `hardware` ( "
		"`id` INTEGER PRIMARY KEY, " // functions as sqlite3 _rowid_ when named *exactly* INTEGER PRIMARY KEY
		"`hardware_id` INTEGER DEFAULT NULL, "
		"`reference` VARCHAR(64) NOT NULL, "
		"`type` INTEGER NOT NULL, "
		"`enabled` INTEGER DEFAULT 0, "
		"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_hardware_reference` ON `hardware`( `reference` )",
		"CREATE INDEX IF NOT EXISTS `ix_hardware_enabled` ON `hardware`( `enabled` )",

		// Devices
		"CREATE TABLE IF NOT EXISTS `devices` ( "
		"`id` INTEGER PRIMARY KEY, "
		"`hardware_id` INTEGER NOT NULL, "
		"`reference` VARCHAR(64) NOT NULL, "
		"`type` INTEGER NOT NULL, "
		"`label` VARCHAR(255) NOT NULL, "
		"`enabled` INTEGER DEFAULT 1, "
		"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_devices_hardware_id_reference` ON `devices`( `hardware_id`, `reference` )",
		"CREATE INDEX IF NOT EXISTS `ix_devices_hardware_id` ON `devices`( `hardware_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_devices_enabled` ON `devices`( `enabled` )",

		// Device History
		"CREATE TABLE IF NOT EXISTS `device_text_history` ( "
		"`device_id` INTEGER NOT NULL, "
		"`value` TEXT NOT NULL, "
		"`date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",
		
		"CREATE INDEX IF NOT EXISTS `ix_device_text_history_device_id` ON `device_text_history`( `device_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_text_history_date` ON `device_text_history`( `date` )",
		
		"CREATE TABLE IF NOT EXISTS `device_counter_history` ( "
		"`device_id` INTEGER NOT NULL, "
		"`value` BIGINT NOT NULL, "
		"`date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE INDEX IF NOT EXISTS `ix_device_counter_history_device_id` ON `device_counter_history`( `device_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_counter_history_date` ON `device_counter_history`( `date` )",

		"CREATE TABLE IF NOT EXISTS `device_level_history` ( "
		"`device_id` INTEGER NOT NULL, "
		"`value` FLOAT NOT NULL, "
		"`date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE INDEX IF NOT EXISTS `ix_device_level_history_device_id` ON `device_level_history`( `device_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_level_history_date` ON `device_level_history`( `date` )",

		"CREATE TABLE IF NOT EXISTS `device_switch_history` ( "
		"`device_id` INTEGER NOT NULL, "
		"`value` VARCHAR(32) NOT NULL, "
		"`date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE INDEX IF NOT EXISTS `ix_device_switch_history_device_id` ON `device_switch_history`( `device_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_switch_history_date` ON `device_switch_history`( `date` )",

		// Device Trends
		"CREATE TABLE IF NOT EXISTS `device_counter_trends` ( "
		"`device_id` INTEGER NOT NULL, "
		"`last` BIGINT NOT NULL, "
		"`diff` BIGINT NOT NULL, "
		"`date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_device_counter_trends_device_id_date` ON `device_counter_trends`( `device_id`, `date` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_counter_trends_device_id` ON `device_counter_trends`( `device_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_counter_trends_date` ON `device_counter_trends`( `date` )",

		"CREATE TABLE IF NOT EXISTS `device_level_trends` ( "
		"`device_id` INTEGER NOT NULL, "
		"`min` FLOAT NOT NULL, "
		"`max` FLOAT NOT NULL, "
		"`average` FLOAT NOT NULL, "
		"`date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_device_level_trends_device_id_date` ON `device_level_trends`( `device_id`, `date` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_level_trends_device_id` ON `device_level_trends`( `device_id` )",
		"CREATE INDEX IF NOT EXISTS `ix_device_level_trends_date` ON `device_level_trends`( `date` )",

		// Settings
		"CREATE TABLE IF NOT EXISTS `settings` ( "
		"`key` VARCHAR(64) NOT NULL, "
		"`value` TEXT NOT NULL, "
		"`last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_settings_key` ON `settings`( `key` )",
		
		"CREATE TABLE IF NOT EXISTS `hardware_settings` ( "
		"`hardware_id` INTEGER NOT NULL, "
		"`key` VARCHAR(64) NOT NULL, "
		"`value` TEXT NOT NULL, "
		"`last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `hardware_id` ) REFERENCES `hardware` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_hardware_settings_hardware_id_key` ON `hardware_settings`( `hardware_id`, `key` )",
		
		"CREATE TABLE IF NOT EXISTS `device_settings` ( "
		"`device_id` INTEGER NOT NULL, "
		"`key` VARCHAR(64) NOT NULL, "
		"`value` TEXT NOT NULL, "
		"`last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",
	
		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_device_settings_device_id_key` ON `device_settings`( `device_id`, `key` )",

		// Scripts
		"CREATE TABLE IF NOT EXISTS `scripts` ( "
		"`id` INTEGER PRIMARY KEY, "
		"`name` VARCHAR(255) NOT NULL, "
		"`code` TEXT, "
		"`language` VARCHAR(64) NOT NULL DEFAULT 'javascript', "
		"`runs` BIGINT DEFAULT 0, "
		"`enabled` INTEGER DEFAULT 1, "
		"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL ) ",
		
		"CREATE TABLE IF NOT EXISTS `x_device_scripts` ( "
		"`device_id` INTEGER NOT NULL, "
		"`script_id` INTEGER NOT NULL, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT, "
		"FOREIGN KEY ( `script_id` ) REFERENCES `scripts` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",
	
		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_device_scripts_device_id_script_id` ON `x_device_scripts`( `device_id`, `script_id` )",

		// Crons
		"CREATE TABLE IF NOT EXISTS `crons` ( "
		"`id` INTEGER PRIMARY KEY, "
		"`name` VARCHAR(255) NOT NULL, "
		"`cron` VARCHAR(64) NOT NULL, "
		"`enabled` INTEGER DEFAULT 1, "
		"`created` TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL )",
		
		"INSERT INTO `crons` (`name`, `cron` ) VALUES ( 'every minute', '* * * * *' )",
		
		"INSERT INTO `crons` (`name`, `cron` ) VALUES ( 'daily at midnight', '0 0 * * *' )",

		"CREATE TABLE IF NOT EXISTS `x_cron_scripts` ( "
		"`cron_id` INTEGER NOT NULL, "
		"`script_id` INTEGER NOT NULL, "
		"FOREIGN KEY ( `cron_id` ) REFERENCES `crons` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT, "
		"FOREIGN KEY ( `script_id` ) REFERENCES `scripts` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE TABLE IF NOT EXISTS `x_cron_devices` ( "
		"`cron_id` INTEGER NOT NULL, "
		"`device_id` INTEGER NOT NULL, "
		"`value` VARCHAR(64) NOT NULL, "
		"FOREIGN KEY ( `cron_id` ) REFERENCES `crons` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT, "
		"FOREIGN KEY ( `device_id` ) REFERENCES `devices` ( `id` ) ON DELETE CASCADE ON UPDATE RESTRICT )",

		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_cron_scripts_cron_id_script_id` ON `x_cron_scripts`( `cron_id`, `script_id` )",
		
		"CREATE UNIQUE INDEX IF NOT EXISTS `ix_cron_devices_cron_id_device_id` ON `x_cron_devices`( `cron_id`, `device_id` )",

	};
	
}; // namespace micasa
