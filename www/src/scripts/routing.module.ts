import { NgModule }             from '@angular/core';
import {
	RouterModule,
	Routes
}                               from '@angular/router';

import { ScriptsListComponent } from './list.component';
import { ScriptEditComponent }  from './edit.component';

import { ScriptResolver }       from './script.resolver';
import { ScriptsListResolver }  from './list.resolver';
import { DevicesListResolver }  from '../devices/list.resolver';

import { SessionGuard }         from '../session/session.guard';
import { ACL }                  from '../users/users.service';

const routes: Routes = [
	{ path: 'scripts',            component: ScriptsListComponent, canActivate: [SessionGuard], resolve: { scripts: ScriptsListResolver }, data: { acl: ACL.Installer } },
	// NOTE a separate route is used to prevent fetching a list of devices for new scripts
	{ path: 'scripts/add',        component: ScriptEditComponent,  canActivate: [SessionGuard], resolve: { script: ScriptResolver }, data: { acl: ACL.Installer } },
	{ path: 'scripts/:script_id', component: ScriptEditComponent,  canActivate: [SessionGuard], resolve: { script: ScriptResolver, devices: DevicesListResolver }, data: { acl: ACL.Installer } },
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
