import { NgModule }             from '@angular/core';
import { RouterModule, Routes } from '@angular/router';

import { ScreenComponent }      from './screen.component';
import { ScreenEditComponent }  from './edit.component';

import { ScreensService }       from './screens.service';
import { UsersService }         from '../users/users.service';

const routes: Routes = [
	{ path: 'dashboard',                 component: ScreenComponent, canActivate: [UsersService], resolve: { screen: ScreensService }, data: { dashboard: true } },
	{ path: 'screen/add',                component: ScreenEditComponent, canActivate: [UsersService], resolve: { screen: ScreensService } },
	{ path: 'screen/:screen_index',      component: ScreenComponent, canActivate: [UsersService], resolve: { screen: ScreensService } },
	{ path: 'screen/:screen_index/edit', component: ScreenEditComponent, canActivate: [UsersService], resolve: { screen: ScreensService } }
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
