import { NgModule }           from '@angular/core';
import { CommonModule }       from '@angular/common';
import { FormsModule }        from '@angular/forms';
import { GridModule }         from '../grid/grid.module';

import { SettingsModule }     from '../settings/settings.module';

import { LinksListComponent } from './list.component';
import { LinkEditComponent }  from './edit.component';
import { LinksService }       from './links.service';
import { LinksListResolver }  from './list.resolver';
import { LinkResolver }       from './link.resolver';
import { LinksRoutingModule } from './routing.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		LinksRoutingModule,
		GridModule,
		SettingsModule
	],
	declarations: [
		LinksListComponent,
		LinkEditComponent
	],
	providers: [
		LinksService,
		LinksListResolver,
		LinkResolver
	],
	exports: [
		// The links components are also used in the device edit component
		LinksListComponent
	]
} )

export class LinksModule {
}
