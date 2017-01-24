import { NgModule }               from '@angular/core';
import { CommonModule }           from '@angular/common';
import { FormsModule }            from '@angular/forms';

import { ScreenComponent }        from './screen.component';
import { ScreenEditComponent }    from './edit.component';
import { ScreensRoutingModule }   from './routing.module';

import { WidgetCounterComponent } from './widgets/counter.component';
import { WidgetLevelComponent }   from './widgets/level.component';
import { WidgetSwitchComponent }  from './widgets/switch.component';
import { WidgetTextComponent }    from './widgets/text.component';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		ScreensRoutingModule
	],
	declarations: [
		ScreenComponent,
		ScreenEditComponent,
		WidgetCounterComponent,
		WidgetLevelComponent,
		WidgetSwitchComponent,
		WidgetTextComponent
	]
} )

export class ScreensModule {
}
