import { NgModule }               from '@angular/core';
import { RouterModule, Routes }   from '@angular/router';

import { DevicesListComponent }   from './list.component';
import { DeviceDetailsComponent } from './details.component';

import { LoginService }           from '../login.service';
import { DevicesService }         from './devices.service';

const routes: Routes = [
	{ path: 'devices',            component: DevicesListComponent,   canActivate: [LoginService] },
	{ path: 'devices/:device_id', component: DeviceDetailsComponent, canActivate: [LoginService], resolve: { device: DevicesService } }
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
