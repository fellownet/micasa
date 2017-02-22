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
import { LinksListResolver }      from './links/list.resolver';
import { LinkResolver }           from './links/link.resolver';
import { LinksListComponent }     from './links/list.component';
import { LinkEditComponent }      from './links/edit.component';

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
		DeviceDetailsComponent,
		LinksListComponent,
		LinkEditComponent
	],
	providers: [
		DevicesService,
		DeviceResolver,
		DevicesListResolver,
		LinksListResolver,
		LinkResolver
	],
	exports: [
		// The devices components are also used in the hardware edit component
		DevicesListComponent,
		DeviceEditComponent
	]
} )

export class DevicesModule {
}
