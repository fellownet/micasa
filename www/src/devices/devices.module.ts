import { NgModule }               from '@angular/core';
import { CommonModule }           from '@angular/common';
import { FormsModule }            from '@angular/forms';
import { GridModule }             from '../grid/grid.module';

import { DevicesListComponent }   from './list.component';
import { DeviceDetailsComponent } from './details.component';
import { DevicesService }         from './devices.service';
import { DevicesRoutingModule }   from './routing.module';
import { WidgetsModule }          from '../widgets/widgets.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		DevicesRoutingModule,
		GridModule,
		WidgetsModule
	],
	declarations: [
		DevicesListComponent,
		DeviceDetailsComponent
	],
	providers: [
		DevicesService
	],
	exports: [
		// The devices components are also used in the hardware details component
		DevicesListComponent,
		DeviceDetailsComponent
	]
} )

export class DevicesModule {
}
