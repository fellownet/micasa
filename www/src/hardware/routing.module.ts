import { NgModule }              from '@angular/core';
import {
	RouterModule,
	Routes
}                                from '@angular/router';

import { HardwareListComponent } from './list.component';
import { HardwareEditComponent } from './edit.component';
import { HardwareListResolver }  from './list.resolver';
import { HardwareResolver }      from './hardware.resolver';

import { DevicesListResolver }   from '../devices/list.resolver';

import { SessionGuard }          from '../session/session.guard';

const routes: Routes = [
	{ path: 'hardware',                   component: HardwareListComponent,  canActivate: [SessionGuard], resolve: { list: HardwareListResolver } },
	{ path: 'hardware/:hardware_id/edit', component: HardwareEditComponent,  canActivate: [SessionGuard], resolve: { list: HardwareListResolver, hardware: HardwareResolver, devices: DevicesListResolver } },
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class HardwareRoutingModule {
}
