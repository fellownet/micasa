import { NgModule }     from '@angular/core';
import {
	RouterModule,
	Routes
}                       from '@angular/router';

import { SessionGuard } from './session/session.guard';

const routes = [
	{ path: '', redirectTo: 'hardware', pathMatch: 'full' }
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
