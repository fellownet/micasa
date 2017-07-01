import { NgModule }              from '@angular/core';
import { CommonModule }          from '@angular/common';
import { FormsModule }           from '@angular/forms';

import { ScreenComponent }       from './screen.component';
import { ScreenEditComponent }   from './edit.component';
import { WidgetComponent }       from './widget.component';
import { ScreensRoutingModule }  from './routing.module';
import { GridModule }            from '../grid/grid.module';
import { UtilsModule }           from '../utils/utils.module';
import { ScreenResolver }        from './screen.resolver';
import { ScreensListResolver }   from './list.resolver';
import { SettingsModule }        from '../settings/settings.module';

import { WidgetChartComponent }  from './widgets/chart.component';
import { WidgetLatestComponent } from './widgets/latest.component';
import { WidgetSwitchComponent } from './widgets/switch.component';
import { WidgetTableComponent }  from './widgets/table.component';
import { WidgetGaugeComponent }  from './widgets/gauge.component';
import { WidgetSliderComponent } from './widgets/slider.component';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		ScreensRoutingModule,
		GridModule,
		UtilsModule,
		SettingsModule
	],
	declarations: [
		ScreenComponent,
		ScreenEditComponent,
		WidgetComponent,
		WidgetChartComponent,
		WidgetLatestComponent,
		WidgetSwitchComponent,
		WidgetTableComponent,
		WidgetGaugeComponent,
		WidgetSliderComponent
	],
	providers: [
		ScreenResolver,
		ScreensListResolver
	]
} )

export class ScreensModule {
}
