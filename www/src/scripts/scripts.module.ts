import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { FormsModule }          from '@angular/forms';
import { GridModule }           from '../grid/grid.module';

import { SettingsModule }       from '../settings/settings.module';
import { ScriptsListComponent } from './list.component';
import { ScriptEditComponent }  from './edit.component';
import { ScriptsService }       from './scripts.service';
import { ScriptResolver }       from './script.resolver';
import { ScriptsListResolver }  from './list.resolver';
import { ScriptsRoutingModule } from './routing.module';

import { DevicesModule }        from '../devices/devices.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		ScriptsRoutingModule,
		DevicesModule,
		GridModule,
		SettingsModule
	],
	declarations: [
		ScriptsListComponent,
		ScriptEditComponent
	],
	providers: [
		ScriptsService,
		ScriptResolver,
		ScriptsListResolver
	]
} )

export class ScriptsModule {
}
