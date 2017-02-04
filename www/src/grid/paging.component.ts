import {
	Component,
	Input
}                        from '@angular/core';
import { GridDirective } from "./grid.directive";

@Component( {
	selector: 'grid-paging',
	template: `
		<ul *ngIf="getPageCount()>1">
			<li *ngIf="getActivePage() > 1" (click)="selectPage(1)">
				<a class="btn btn-default"><i class="fa fa-angle-double-left"></i></a>
			</li>
			<li *ngIf="getActivePage() > 4 && getActivePage() + 1 > getPageCount()" (click)="selectPage(getActivePage() - 4)">
				<a class="btn btn-default">{{getActivePage()-4}}</a>
			</li>
			<li *ngIf="getActivePage() > 3 && getActivePage() + 2 > getPageCount()" (click)="selectPage(getActivePage() - 3)">
				<a class="btn btn-default">{{getActivePage()-3}}</a>
			</li>
			<li *ngIf="getActivePage() > 2" (click)="selectPage(getActivePage() - 2)">
				<a class="btn btn-default">{{getActivePage()-2}}</a>
			</li>
			<li *ngIf="getActivePage() > 1" (click)="selectPage(getActivePage() - 1)">
				<a class="btn btn-default">{{getActivePage()-1}}</a>
			</li>
			<li>
				<a class="btn btn-primary">{{getActivePage()}}</a>
			</li>
			<li *ngIf="getActivePage() + 1 <= getPageCount()" (click)="selectPage(getActivePage() + 1)">
				<a class="btn btn-default">{{getActivePage()+1}}</a>
			</li>
			<li *ngIf="getActivePage() + 2 <= getPageCount()" (click)="selectPage(getActivePage() + 2)">
				<a class="btn btn-default">{{getActivePage()+2}}</a>
			</li>
			<li *ngIf="getActivePage() + 3 <= getPageCount() && getActivePage() < 3" (click)="selectPage(getActivePage() + 3)">
				<a class="btn btn-default">{{getActivePage()+3}}</a>
			</li>
			<li *ngIf="getActivePage() + 4 <= getPageCount() && getActivePage() < 2" (click)="selectPage(getActivePage() + 4)">
				<a class="btn btn-default">{{getActivePage()+4}}</a>
			</li>
			<li *ngIf="getActivePage() < getPageCount()" (click)="selectPage(getPageCount())">
				<a class="btn btn-default"><i class="fa fa-angle-double-right"></i></a>
			</li>
		</ul>
		`
} )

export class GridPagingComponent {

	@Input( "grid" ) private _grid: GridDirective;

	public selectPage( page_: number ) {
		this._grid.setPage( page_ );
	};

	public getActivePage(): number {
		return this._grid.getPage();
	};

	public getPageCount(): number {
		return Math.ceil( this._grid.data.length / this._grid.getPageSize() );
	};
}
