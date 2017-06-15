import { NgModule }               from '@angular/core';
import { RouterModule, Routes }   from '@angular/router';

import { ScreenComponent }        from './screen.component';
import { ScreenEditComponent }    from './edit.component';
import { ScreenResolver }         from './screen.resolver';
import { ScreensListResolver }    from './list.resolver';

import { DeviceEditComponent }    from '../devices/edit.component';
import { DeviceResolver }         from '../devices/device.resolver';

import { SessionGuard }           from '../session/session.guard';

const routes: Routes = [
	{ path: 'dashboard',               component: ScreenComponent,     canActivate: [SessionGuard], resolve: { payload: ScreenResolver }, data: { dashboard: true } },
	{ path: 'screens/add',             component: ScreenEditComponent, canActivate: [SessionGuard], resolve: { payload: ScreenResolver } },
	{ path: 'screens/:screen_id',      component: ScreenComponent,     canActivate: [SessionGuard], resolve: { payload: ScreenResolver } },
	{ path: 'screens/:screen_id/edit', component: ScreenEditComponent, canActivate: [SessionGuard], resolve: { payload: ScreenResolver } },
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
