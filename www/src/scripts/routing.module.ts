import { NgModule }                 from '@angular/core';
import { RouterModule, Routes }     from '@angular/router';

import { ScriptsListComponent }     from './list.component';
import { ScriptDetailsComponent }   from './details.component';
import { DeviceDetailsComponent }   from '../devices/details.component';

import { UsersService }             from '../users/users.service';
import { ScriptsService }           from './scripts.service';

const routes: Routes = [
	{ path: 'scripts',            component: ScriptsListComponent,   canActivate: [UsersService] },
	{ path: 'scripts/:script_id', component: ScriptDetailsComponent, canActivate: [UsersService], resolve: { script: ScriptsService } },
];

@NgModule( {
	imports: [
		RouterModule.forChild( routes )
	],
	exports: [
		RouterModule
	]
} )

export class ScriptsRoutingModule {
}
