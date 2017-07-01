import { NgModule }            from '@angular/core';
import { CommonModule }        from '@angular/common';
import { FormsModule }         from '@angular/forms';

import { GridDirective }       from './grid.directive';
import { GridColumnComponent } from './column.component';
import { GridPagingComponent } from './paging.component';
import { GridSearchComponent } from './search.component';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule
	],
	declarations: [
		GridDirective,
		GridColumnComponent,
		GridPagingComponent,
		GridSearchComponent
	],
	exports: [
		GridDirective,
		GridColumnComponent,
		GridPagingComponent,
		GridSearchComponent
	]
} )

export class GridModule {
}
