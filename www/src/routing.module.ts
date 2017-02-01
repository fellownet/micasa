import { NgModule }      from '@angular/core';
import {
	RouterModule,
	Routes
}                        from '@angular/router';

import { HelpComponent } from './help.component';
import { UsersService }  from './users/users.service';

const routes = [
	{ path: '',     redirectTo: 'dashboard', pathMatch: 'full' },
	{ path: 'help', component: HelpComponent, canActivate: [UsersService] },
];

@NgModule( {
	imports: [
		RouterModule.forRoot( routes, { useHash: true } )
	],
	exports: [
		RouterModule
	]
} )

export class RoutingModule {
}
