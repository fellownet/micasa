import { NgModule }       from '@angular/core';
import {
	RouterModule,
	Routes
}                         from '@angular/router';

import { LoginComponent } from './login.component';
import { ErrorComponent } from './error.component';
import {
	ErrorResolver
}                         from './error.resolver';

const routes: Routes = [
	{ path: 'login', component: LoginComponent },
	{ path: 'error', component: ErrorComponent, resolve: { error: ErrorResolver } }
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class SessionRoutingModule {
}
