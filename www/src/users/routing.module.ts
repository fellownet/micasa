import { NgModule }           from '@angular/core';
import {
	RouterModule,
	Routes
}                             from '@angular/router';

import { LoginComponent }     from './login.component';
import { UsersListComponent } from './list.component';
import { UserEditComponent }  from './edit.component';

import { UserResolver }       from './user.resolver';
import { UsersListResolver }  from './list.resolver';

import { UsersService }       from './users.service';

const routes: Routes = [
	{ path: 'login',          component: LoginComponent },
	{ path: 'users',          component: UsersListComponent, canActivate: [UsersService], resolve: { users: UsersListResolver } },
	{ path: 'users/:user_id', component: UserEditComponent,  canActivate: [UsersService], resolve: { user: UserResolver } },
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class UsersRoutingModule {
}
