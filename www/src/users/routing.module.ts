import { NgModule }            from '@angular/core';
import {
	RouterModule,
	Routes
}                              from '@angular/router';

import { UsersListComponent }  from './list.component';
import { UserEditComponent }   from './edit.component';

import { UserResolver }        from './user.resolver';
import { UsersListResolver }   from './list.resolver';

import { SessionGuard }        from '../session/session.guard';

const routes: Routes = [
	{ path: 'users',          component: UsersListComponent, canActivate: [SessionGuard], resolve: { users: UsersListResolver } },
	{ path: 'users/:user_id', component: UserEditComponent,  canActivate: [SessionGuard], resolve: { user: UserResolver } },
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
