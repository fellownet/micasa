import { NgModule }           from '@angular/core';
import {
	RouterModule,
	Routes
}                             from '@angular/router';

import { LinksListComponent } from './list.component';
import { LinkEditComponent }  from './edit.component';
import { LinksListResolver }  from './list.resolver';
import { LinkResolver }       from './link.resolver';

import { DeviceResolver }     from '../devices/device.resolver';

import { SessionGuard }       from '../session/session.guard';
import { ACL }                from '../users/users.service';

const routes: Routes = [
	{ path: 'links',                            component: LinksListComponent, canActivate: [SessionGuard], resolve: { links: LinksListResolver }, data: { acl: ACL.Installer } },
	{ path: 'links/:link_id',                   component: LinkEditComponent,  canActivate: [SessionGuard], resolve: { link: LinkResolver }, data: { acl: ACL.Installer } },
	{ path: 'links/:link_id/device/:device_id', component: LinkEditComponent,  canActivate: [SessionGuard], resolve: { device: DeviceResolver, link: LinkResolver }, data: { acl: ACL.Installer } }
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class LinksRoutingModule {
}
