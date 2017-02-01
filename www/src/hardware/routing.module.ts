import { NgModule }               from '@angular/core';
import {
	RouterModule,
	Routes
}                                 from '@angular/router';

import { HardwareListComponent }  from './list.component';
import { HardwareEditComponent }  from './edit.component';
import { DeviceDetailsComponent } from '../devices/details.component';
import { DeviceEditComponent }    from '../devices/edit.component';

import { HardwareListResolver }   from './list.resolver';
import { HardwareResolver }       from './hardware.resolver';
import { ScriptsListResolver }    from '../scripts/list.resolver';
import { DevicesListResolver }    from '../devices/list.resolver';
import { DeviceResolver }         from '../devices/device.resolver';
import { DataResolver }           from '../devices/data.resolver';
import { TimersListResolver }     from '../timers/list.resolver';

import { UsersService }           from '../users/users.service';

const routes: Routes = [
	{ path: 'hardware',                                                component: HardwareListComponent,  canActivate: [UsersService], resolve: { list: HardwareListResolver } },
	{ path: 'hardware/:hardware_id/edit',                              component: HardwareEditComponent,  canActivate: [UsersService], resolve: { list: HardwareListResolver, hardware: HardwareResolver, devices: DevicesListResolver } },
	{ path: 'hardware/:parent_id/:hardware_id/edit',                   component: HardwareEditComponent,  canActivate: [UsersService], resolve: { list: HardwareListResolver, hardware: HardwareResolver, devices: DevicesListResolver } },
	{ path: 'hardware/:hardware_id/device/:device_id',                 component: DeviceDetailsComponent, canActivate: [UsersService], resolve: { hardware: HardwareResolver, device: DeviceResolver, data: DataResolver } },
	{ path: 'hardware/:parent_id/:hardware_id/device/:device_id',      component: DeviceDetailsComponent, canActivate: [UsersService], resolve: { hardware: HardwareResolver, device: DeviceResolver, data: DataResolver } },
	{ path: 'hardware/:hardware_id/device/:device_id/edit',            component: DeviceEditComponent,    canActivate: [UsersService], resolve: { hardware: HardwareResolver, device: DeviceResolver, scripts: ScriptsListResolver, timers: TimersListResolver } },
	{ path: 'hardware/:parent_id/:hardware_id/device/:device_id/edit', component: DeviceEditComponent,    canActivate: [UsersService], resolve: { hardware: HardwareResolver, device: DeviceResolver, scripts: ScriptsListResolver, timers: TimersListResolver } }
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
