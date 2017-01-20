import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { FormsModule }          from '@angular/forms';

import { ScreenComponent }      from './screen.component';
import { ScreenEditComponent }  from './edit.component';
import { ScreensRoutingModule } from './routing.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		ScreensRoutingModule
	],
	declarations: [
		ScreenComponent,
		ScreenEditComponent
	]
} )

export class ScreensModule {
}
