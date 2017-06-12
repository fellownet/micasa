import { NgModule }               from '@angular/core';
import { CommonModule }           from '@angular/common';
import { FormsModule }            from '@angular/forms';

import { GridModule }             from '../grid/grid.module';
import { SettingsModule }         from '../settings/settings.module';
import { UtilsModule }            from '../utils/utils.module';

import { DevicesListComponent }   from './list.component';
import { DeviceEditComponent }    from './edit.component';
import { DevicesService }         from './devices.service';
import { DevicesRoutingModule }   from './routing.module';
import { DeviceResolver }         from './device.resolver';
import { DevicesListResolver }    from './list.resolver';

import { TimersModule }           from '../timers/timers.module';
import { LinksModule }            from '../links/links.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		DevicesRoutingModule,
		GridModule,
		SettingsModule,
		TimersModule,
		LinksModule,
		UtilsModule
	],
	declarations: [
		DevicesListComponent,
		DeviceEditComponent
	],
	providers: [
		DevicesService,
		DeviceResolver,
		DevicesListResolver
	],
	exports: [
		// The devices components are also used in the hardware edit component
		DevicesListComponent,
		DeviceEditComponent
	]
} )

export class DevicesModule {
}
