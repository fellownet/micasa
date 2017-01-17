import { NgModule }               from '@angular/core';
import { RouterModule, Routes }   from '@angular/router';

import { DevicesListComponent }   from './list.component';
import { DeviceDetailsComponent } from './details.component';

import { UsersService }           from '../users/users.service';
import { DevicesService }         from './devices.service';

const routes: Routes = [
	{ path: 'devices',            component: DevicesListComponent,   canActivate: [UsersService] },
	{ path: 'devices/:device_id', component: DeviceDetailsComponent, canActivate: [UsersService], resolve: { device: DevicesService } }
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
