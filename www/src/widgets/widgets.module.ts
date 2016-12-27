import { NgModule }               from '@angular/core';
import { CommonModule }           from '@angular/common';
import { FormsModule }            from '@angular/forms';

import { WidgetCounterComponent } from './counter.component';
import { WidgetLevelComponent }   from './level.component';
import { WidgetSwitchComponent }  from './switch.component';
import { WidgetTextComponent }    from './text.component';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule
	],
	declarations: [
		WidgetCounterComponent,
		WidgetLevelComponent,
		WidgetSwitchComponent,
		WidgetTextComponent
	],
	exports: [
		WidgetCounterComponent,
		WidgetLevelComponent,
		WidgetSwitchComponent,
		WidgetTextComponent
	]
} )

export class WidgetsModule {
}
