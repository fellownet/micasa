import { NgModule }               from '@angular/core';
import { RouterModule, Routes }   from '@angular/router';

import { ScreenComponent }        from './screen.component';
import { ScreenEditComponent }    from './edit.component';
import { DeviceDetailsComponent } from '../devices/details.component';
import { DeviceEditComponent }    from '../devices/edit.component';

import { ScreenResolver }         from './screen.resolver';
import { ScreensListResolver }    from './list.resolver';
import { DeviceResolver }         from '../devices/device.resolver';
import { ScriptsListResolver }    from '../scripts/list.resolver';

import { SessionGuard }           from '../session/session.guard';

const routes: Routes = [
	{ path: 'dashboard',                                 component: ScreenComponent,        canActivate: [SessionGuard], resolve: { screen: ScreenResolver }, data: { dashboard: true } },
	{ path: 'screens/add',                               component: ScreenEditComponent,    canActivate: [SessionGuard], resolve: { screen: ScreenResolver } },
	{ path: 'screens/:screen_id',                        component: ScreenComponent,        canActivate: [SessionGuard], resolve: { screen: ScreenResolver } },
	{ path: 'screens/:screen_id/edit',                   component: ScreenEditComponent,    canActivate: [SessionGuard], resolve: { screen: ScreenResolver } },
	{ path: 'screens/:screen_id/device/:device_id',      component: DeviceDetailsComponent, canActivate: [SessionGuard], resolve: { screen: ScreenResolver, device: DeviceResolver } },
	{ path: 'screens/:screen_id/device/:device_id/edit', component: DeviceEditComponent,    canActivate: [SessionGuard], resolve: { screen: ScreenResolver, device: DeviceResolver, scripts: ScriptsListResolver, screens: ScreensListResolver } }
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class ScreensRoutingModule {
}
