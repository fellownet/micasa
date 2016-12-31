import { NgModule }              from '@angular/core';
import { CommonModule }          from '@angular/common';
import { FormsModule }           from '@angular/forms';
import { GridModule }            from '../grid/grid.module';

import { TimersListComponent }   from './list.component';
import { TimerDetailsComponent } from './details.component';
import { TimersService }         from './timers.service';
import { TimersRoutingModule }   from './routing.module';

import { ScriptsModule }         from '../scripts/scripts.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		TimersRoutingModule,
		ScriptsModule,
		GridModule
	],
	declarations: [
		TimersListComponent,
		TimerDetailsComponent
	],
	providers: [
		TimersService
	]
} )

export class TimersModule {
}
