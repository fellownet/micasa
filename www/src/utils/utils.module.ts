import { NgModule }      from '@angular/core';

import { TimestampPipe } from './timestamp.pipe';
import { TruncatePipe }  from './truncate.pipe';

@NgModule( {
	imports: [
	],
	declarations: [
		TimestampPipe,
		TruncatePipe
	],
	exports: [
		TimestampPipe,
		TruncatePipe
	]
} )

export class UtilsModule {
}
