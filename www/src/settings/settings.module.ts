import { NgModule }          from '@angular/core';
import { CommonModule }      from '@angular/common';
import { FormsModule }       from '@angular/forms';

import { UtilsModule }       from '../utils/utils.module';

import { SettingsComponent } from './settings.component';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		UtilsModule
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
