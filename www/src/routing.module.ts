import { NgModule }             from '@angular/core';

import { RouterModule, Routes } from '@angular/router';

import { HelpComponent }        from './help.component';

// Route Guards
import { UsersService }         from './users/users.service';

// http://www.gistia.com/working-angular-2-router-part-1/
// https://angular.io/docs/ts/latest/guide/router.html
// http://jasonwatmore.com/post/2016/08/16/angular-2-jwt-authentication-example-tutorial
const routes = [
	{ path: '',            redirectTo: 'dashboard', pathMatch: 'full' },
	{ path: 'help',        component: HelpComponent, canActivate: [UsersService] },
];

@NgModule( {
	imports   : [ RouterModule.forRoot( routes, { useHash: true } ) ],
	exports   : [ RouterModule ]
} )

export class RoutingModule {
}
