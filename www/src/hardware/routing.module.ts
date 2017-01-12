import { NgModule }                 from '@angular/core';
import { RouterModule, Routes }     from '@angular/router';

import { HardwareListComponent }    from './list.component';
import { HardwareDetailsComponent } from './details.component';
import { DeviceDetailsComponent }   from '../devices/details.component';

import { UsersService }             from '../users/users.service';
import { HardwareService }          from './hardware.service';
import { DevicesService }           from '../devices/devices.service';

const routes: Routes = [
	{ path: 'hardware',                         component: HardwareListComponent,    canActivate: [UsersService] },
	{ path: 'hardware/:hardware_id',            component: HardwareDetailsComponent, canActivate: [UsersService], resolve: { hardware: HardwareService } },
	{ path: 'hardware/:hardware_id/:device_id', component: DeviceDetailsComponent,   canActivate: [UsersService], resolve: { device: DevicesService } }
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
