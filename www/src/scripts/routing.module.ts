import { NgModule }                 from '@angular/core';
import { RouterModule, Routes }     from '@angular/router';

import { ScriptsListComponent }     from './list.component';
import { ScriptDetailsComponent }   from './details.component';
import { DeviceDetailsComponent }   from '../devices/details.component';

import { LoginService }             from '../login.service';
import { ScriptsService }           from './scripts.service';

const routes: Routes = [
	{ path: 'scripts',            component: ScriptsListComponent,   canActivate: [LoginService] },
	{ path: 'scripts/:script_id', component: ScriptDetailsComponent, canActivate: [LoginService], resolve: { script: ScriptsService } },
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
