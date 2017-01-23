import { NgModule }                 from '@angular/core';
import { CommonModule }             from '@angular/common';
import { FormsModule }              from '@angular/forms';

import { GridModule }               from '../grid/grid.module';
import { SettingsModule }           from '../settings/settings.module';

import { HardwareListComponent }    from './list.component';
import { HardwareDetailsComponent } from './details.component';
import { HardwareService }          from './hardware.service';
import { HardwareRoutingModule }    from './routing.module';

import { DevicesModule }            from '../devices/devices.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		HardwareRoutingModule,
		DevicesModule,
		GridModule,
		SettingsModule
	],
	declarations: [
		HardwareListComponent,
		HardwareDetailsComponent
	],
	providers: [
		HardwareService
	]
} )

export class HardwareModule {
}
