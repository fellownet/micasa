import { NgModule }             from '@angular/core';
import {
	RouterModule,
	Routes
}                               from '@angular/router';

import { PluginsListComponent } from './list.component';
import { PluginEditComponent }  from './edit.component';
import { PluginsListResolver }  from './list.resolver';
import { PluginResolver }       from './plugin.resolver';

import { DevicesListResolver }  from '../devices/list.resolver';

import { SessionGuard }         from '../session/session.guard';
import { ACL }                  from '../users/users.service';

const routes: Routes = [
	{ path: 'plugins',            component: PluginsListComponent, canActivate: [SessionGuard], resolve: { plugins: PluginsListResolver }, data: { acl: ACL.Installer } },
	// NOTE a separate route is used to prevent fetching a list of devices for new plugins
	{ path: 'plugins/add',        component: PluginEditComponent,  canActivate: [SessionGuard], resolve: { plugin: PluginResolver }, data: { acl: ACL.Installer } },
	{ path: 'plugins/:plugin_id', component: PluginEditComponent,  canActivate: [SessionGuard], resolve: { plugin: PluginResolver, devices: DevicesListResolver }, data: { acl: ACL.Installer } }
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class PluginsRoutingModule {
}
