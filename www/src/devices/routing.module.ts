import { NgModule }               from '@angular/core';
import {
	RouterModule,
	Routes
}                                 from '@angular/router';

import { DevicesListComponent }   from './list.component';
import { DeviceDetailsComponent } from './details.component';
import { DeviceEditComponent }    from './edit.component';
import { TimerEditComponent }     from '../timers/edit.component';
import { LinkEditComponent }      from './links/edit.component';
import { ScreenComponent }        from '../screens/screen.component';

import { ScriptsListResolver }    from '../scripts/list.resolver';
import { DeviceResolver }         from './device.resolver';
import { DevicesListResolver }    from './list.resolver';
import { LinksListResolver }      from './links/list.resolver';
import { LinkResolver }           from './links/link.resolver';
import { TimersListResolver }     from '../timers/list.resolver';
import { TimerResolver }          from '../timers/timer.resolver';
import { ScreenResolver }         from '../screens/screen.resolver';
import { ScreensListResolver }    from '../screens/list.resolver';

import { SessionGuard }           from '../session/session.guard';

const routes: Routes = [
	{ path: 'devices',                             component: DevicesListComponent,   canActivate: [SessionGuard], resolve: { devices: DevicesListResolver } },
	{ path: 'devices/:device_id',                  component: ScreenComponent,        canActivate: [SessionGuard], resolve: { payload: ScreenResolver } },
	{ path: 'devices/:device_id/edit',             component: DeviceEditComponent,    canActivate: [SessionGuard], resolve: { device: DeviceResolver, scripts: ScriptsListResolver, timers: TimersListResolver, screens: ScreensListResolver, links: LinksListResolver } },
	{ path: 'devices/:device_id/timers/:timer_id', component: TimerEditComponent,     canActivate: [SessionGuard], resolve: { device: DeviceResolver, timer: TimerResolver } },
	{ path: 'devices/:device_id/links/:link_id',   component: LinkEditComponent,      canActivate: [SessionGuard], resolve: { device: DeviceResolver, link: LinkResolver } }
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
