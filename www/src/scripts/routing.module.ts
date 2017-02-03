import { NgModule }               from '@angular/core';
import {
	RouterModule,
	Routes
}                                 from '@angular/router';

import { ScriptsListComponent }   from './list.component';
import { ScriptEditComponent }    from './edit.component';
import { DeviceDetailsComponent } from '../devices/details.component';
import { DeviceEditComponent }    from '../devices/edit.component';

import { ScriptResolver }         from './script.resolver';
import { ScriptsListResolver }    from './list.resolver';
import { DeviceResolver }         from '../devices/device.resolver';
import { DevicesListResolver }    from '../devices/list.resolver';
import { DataResolver }           from '../devices/data.resolver';

import { SessionGuard }           from '../session/session.guard';

const routes: Routes = [
	{ path: 'scripts',                                   component: ScriptsListComponent,   canActivate: [SessionGuard], resolve: { scripts: ScriptsListResolver } },
	{ path: 'scripts/:script_id',                        component: ScriptEditComponent,    canActivate: [SessionGuard], resolve: { script: ScriptResolver, devices: DevicesListResolver } },
	{ path: 'scripts/:script_id/device/:device_id',      component: DeviceDetailsComponent, canActivate: [SessionGuard], resolve: { script: ScriptResolver, device: DeviceResolver, data: DataResolver } },
	{ path: 'scripts/:script_id/device/:device_id/edit', component: DeviceEditComponent,    canActivate: [SessionGuard], resolve: { script: ScriptResolver, device: DeviceResolver, scripts: ScriptsListResolver } }
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class ScriptsRoutingModule {
}
