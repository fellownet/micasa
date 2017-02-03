import { NgModule }             from '@angular/core';
import { RouterModule, Routes } from '@angular/router';

import { ScreenComponent }      from './screen.component';
import { ScreenEditComponent }  from './edit.component';

import { ScreenResolver }       from './screen.resolver';

import { SessionGuard }         from '../session/session.guard';

const routes: Routes = [
	{ path: 'dashboard',              component: ScreenComponent,     canActivate: [SessionGuard], resolve: { screen: ScreenResolver }, data: { dashboard: true } },
	{ path: 'screen/add',             component: ScreenEditComponent, canActivate: [SessionGuard], resolve: { screen: ScreenResolver } },
	{ path: 'screen/:screen_id',      component: ScreenComponent,     canActivate: [SessionGuard], resolve: { screen: ScreenResolver } },
	{ path: 'screen/:screen_id/edit', component: ScreenEditComponent, canActivate: [SessionGuard], resolve: { screen: ScreenResolver } }
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
