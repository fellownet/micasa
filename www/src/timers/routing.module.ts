import { NgModule }              from '@angular/core';
import { RouterModule, Routes }  from '@angular/router';

import { TimersListComponent }   from './list.component';
import { TimerDetailsComponent } from './details.component';

import { LoginService }          from '../login.service';
import { TimersService }         from './timers.service';

const routes: Routes = [
	{ path: 'timers',            component: TimersListComponent,   canActivate: [LoginService] },
	{ path: 'timers/:timer_id', component: TimerDetailsComponent, canActivate: [LoginService], resolve: { timer: TimersService } },
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
