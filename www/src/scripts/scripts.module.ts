import { NgModule }                 from '@angular/core';
import { CommonModule }             from '@angular/common';
import { FormsModule }              from '@angular/forms';
import { GridModule }               from '../grid/grid.module';

import { ScriptsListComponent }     from './list.component';
import { ScriptDetailsComponent }   from './details.component';
import { ScriptsService }           from './scripts.service';
import { ScriptsRoutingModule }     from './routing.module';

import { DevicesModule }            from '../devices/devices.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		ScriptsRoutingModule,
		DevicesModule,
		GridModule
	],
	declarations: [
		ScriptsListComponent,
		ScriptDetailsComponent
	],
	providers: [
		ScriptsService
	]
} )

export class ScriptsModule {
}
