import { NgModule }            from '@angular/core';
import { CommonModule }        from '@angular/common';
import { FormsModule }         from '@angular/forms';
import { GridModule }          from '../grid/grid.module';

import { TimersListComponent } from './list.component';
import { TimerEditComponent }  from './edit.component';
import { TimersService }       from './timers.service';
import { TimersListResolver }  from './list.resolver';
import { TimerResolver }       from './timer.resolver';
import { TimersRoutingModule } from './routing.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		TimersRoutingModule,
		GridModule
	],
	declarations: [
		TimersListComponent,
		TimerEditComponent
	],
	providers: [
		TimersService,
		TimersListResolver,
		TimerResolver
	],
	exports: [
		// The timers components are also used in the device edit component
		TimersListComponent
	]
} )

export class TimersModule {
}
