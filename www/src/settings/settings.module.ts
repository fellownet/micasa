import { NgModule }          from '@angular/core';
import { CommonModule }      from '@angular/common';
import { FormsModule }       from '@angular/forms';

import { SettingsComponent } from './settings.component';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule
	],
	declarations: [
		SettingsComponent
	],
	exports: [
		SettingsComponent
	]
} )

export class SettingsModule {
}
