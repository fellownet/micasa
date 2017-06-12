import { NgModule }      from '@angular/core';

import { TimestampPipe } from './timestamp.pipe';
import { TruncatePipe }  from './truncate.pipe';
import { SortPipe }      from './sort.pipe';

@NgModule( {
	imports: [
	],
	declarations: [
		TimestampPipe,
		TruncatePipe,
		SortPipe
	],
	exports: [
		TimestampPipe,
		TruncatePipe,
		SortPipe
	]
} )

export class UtilsModule {
}
