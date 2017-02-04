import { NgModule }      from '@angular/core';

import { TimestampPipe } from './timestamp.pipe';

@NgModule( {
	imports: [
	],
	declarations: [
		TimestampPipe
	],
	exports: [
		TimestampPipe
	]
} )

export class UtilsModule {
}
