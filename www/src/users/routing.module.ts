import { NgModule }               from '@angular/core';
import { RouterModule, Routes }   from '@angular/router';

import { LoginComponent }         from './login.component';
import { UsersListComponent }     from './list.component';
import { UserDetailsComponent }   from './details.component';

import { UsersService }           from './users.service';

const routes: Routes = [
	{ path: 'login',          component: LoginComponent },
	{ path: 'users',          component: UsersListComponent,   canActivate: [UsersService] },
	{ path: 'users/:user_id', component: UserDetailsComponent, canActivate: [UsersService], resolve: { user: UsersService } },
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
