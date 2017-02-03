import { NgModule }               from '@angular/core';
import {
	RouterModule,
	Routes
}                                 from '@angular/router';

import { DevicesListComponent }   from './list.component';
import { DeviceDetailsComponent } from './details.component';
import { DeviceEditComponent }    from './edit.component';
import { TimerEditComponent }     from '../timers/edit.component';

import { ScriptsListResolver }    from '../scripts/list.resolver';
import { DeviceResolver }         from './device.resolver';
import { DevicesListResolver }    from './list.resolver';
import { DataResolver }           from './data.resolver';
import { TimersListResolver }     from '../timers/list.resolver';
import { TimerResolver }          from '../timers/timer.resolver';

import { SessionGuard }           from '../session/session.guard';

const routes: Routes = [
	{ path: 'devices',                             component: DevicesListComponent,   canActivate: [SessionGuard], resolve: { devices: DevicesListResolver } },
	{ path: 'devices/:device_id',                  component: DeviceDetailsComponent, canActivate: [SessionGuard], resolve: { device: DeviceResolver, data: DataResolver } },
	{ path: 'devices/:device_id/edit',             component: DeviceEditComponent,    canActivate: [SessionGuard], resolve: { device: DeviceResolver, scripts: ScriptsListResolver, timers: TimersListResolver } },
	{ path: 'devices/:device_id/timers/:timer_id', component: TimerEditComponent,     canActivate: [SessionGuard], resolve: { device: DeviceResolver, timer: TimerResolver } }
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class DevicesRoutingModule {
}
