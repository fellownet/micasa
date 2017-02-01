import { NgModule }            from '@angular/core';
import {
	RouterModule,
	Routes
}                              from '@angular/router';

import { TimersListComponent } from './list.component';
import { TimerEditComponent }  from './edit.component';

import { TimersListResolver }  from './list.resolver';
import { TimerResolver }       from './timer.resolver';
import { ScriptsListResolver } from '../scripts/list.resolver';

import { UsersService }        from '../users/users.service';

const routes: Routes = [
	{ path: 'timers',           component: TimersListComponent, canActivate: [UsersService], resolve: { timers: TimersListResolver } },
	{ path: 'timers/:timer_id', component: TimerEditComponent,  canActivate: [UsersService], resolve: { timer: TimerResolver, scripts: ScriptsListResolver } },
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class TimersRoutingModule {
}
