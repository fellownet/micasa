import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { FormsModule }          from '@angular/forms';

import { GridModule }           from '../grid/grid.module';
import { SettingsModule }       from '../settings/settings.module';
import { PluginsListComponent } from './list.component';
import { PluginEditComponent }  from './edit.component';
import { PluginsService }       from './plugins.service';
import { PluginsRoutingModule } from './routing.module';
import { PluginsListResolver }  from './list.resolver';
import { PluginResolver }       from './plugin.resolver';
import { DevicesModule }        from '../devices/devices.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		PluginsRoutingModule,
		DevicesModule,
		GridModule,
		SettingsModule
	],
	declarations: [
		PluginsListComponent,
		PluginEditComponent
	],
	providers: [
		PluginsService,
		PluginsListResolver,
		PluginResolver
	]
} )

export class PluginsModule {
}
