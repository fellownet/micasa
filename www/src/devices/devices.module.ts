import { NgModule }               from '@angular/core';
import { CommonModule }           from '@angular/common';
import { FormsModule }            from '@angular/forms';

import { GridModule }             from '../grid/grid.module';
import { SettingsModule }         from '../settings/settings.module';
import { UtilsModule }            from '../utils/utils.module';

import { DevicesListComponent }   from './list.component';
import { DeviceDetailsComponent } from './details.component';
import { DeviceEditComponent }    from './edit.component';
import { DevicesService }         from './devices.service';
import { DevicesRoutingModule }   from './routing.module';
import { DeviceResolver }         from './device.resolver';
import { DevicesListResolver }    from './list.resolver';
import { DataResolver }           from './data.resolver';

import { TimersModule }           from '../timers/timers.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		DevicesRoutingModule,
		GridModule,
		SettingsModule,
		TimersModule,
		UtilsModule
	],
	declarations: [
		DevicesListComponent,
		DeviceEditComponent,
		DeviceDetailsComponent
	],
	providers: [
		DevicesService,
		DeviceResolver,
		DevicesListResolver,
		DataResolver
	],
	exports: [
		// The devices components are also used in the hardware edit component
		DevicesListComponent,
		DeviceEditComponent
	]
} )

export class DevicesModule {
}
