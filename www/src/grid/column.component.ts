import {
	Component,
	Input
}                        from '@angular/core';

import { GridDirective } from "./grid.directive";

@Component( {
	selector: 'th[gridField]',
	template: `
		<a (click)="sort()">
			<ng-content></ng-content>
			<i class="fa fa-sort-alpha-asc" *ngIf="isSortedByMe()=='asc'"></i>
			<i class="fa fa-sort-alpha-desc" *ngIf="isSortedByMe()=='desc'"></i>
		</a>`
} )

export class GridColumnComponent {

	@Input( 'gridField' ) public field: string;

	public constructor(
		private _grid: GridDirective
	) {
	};

	public sort() {
		if ( this._grid.getSortField() == this.field ) {
			this._grid.setSort( this.field, this._grid.getSortOrder() == 'asc' ? 'desc' : 'asc' );
		} else {
			this._grid.setSort( this.field, 'asc' );
		}
	};

	public isSortedByMe(): any {
		if ( this._grid.getSortField() == this.field ) {
			return this._grid.getSortOrder();
		} else {
			return false;
		}
	};
}
