import {
	Directive,
	Input,
	OnInit,
	OnChanges,
	SimpleChanges
}                 from '@angular/core';

import {
	SortPipe
}                 from '../utils/sort.pipe';

@Directive( {
	selector: 'table[gridData]',
	exportAs: 'GridDirective'
} )

export class GridDirective implements OnInit, OnChanges {

	@Input( "gridData" ) public data: any[] = [];

	@Input( "gridSort" ) private _defaultSortField: string;
	@Input( "gridOrder" ) private _defaultSortOrder: string;
	@Input( "gridPage" ) private _page: number = 1;
	@Input( "gridPageSize" ) private _pageSize: number = 15;

	private _sortField: string = '';
	private _sortOrder: string = 'asc';

	public page: any[] = [];

	public constructor(
	) {
	};

	public ngOnChanges( changes_: SimpleChanges ) {
		if ( this._defaultSortField ) {
			this._sortField = this._defaultSortField;
			if ( this._defaultSortOrder ) {
				this._sortOrder = this._defaultSortOrder.toLowerCase() == 'desc' ? 'desc' : 'asc';
			}
			new SortPipe().transform( this.data, this._sortField, this._sortOrder == 'desc' );
		}
		this._slice();
	};

	public ngOnInit() {
	};

	public setSort( sortField_: string, sortOrder_: string ) {
		if (
			this._sortField != sortField_
			|| this._sortOrder != sortOrder_
		) {
			this._sortField = sortField_;
			this._sortOrder = sortOrder_;
			new SortPipe().transform( this.data, this._sortField, this._sortOrder == 'desc' );
			this._slice();
		}
	};

	public setPage( page_: number ) {
		this._page = page_;
		this._slice();
	};

	public getPage(): number {
		return this._page;
	};

	public getSortField(): string {
		return this._sortField;
	};

	public getSortOrder(): string {
		return this._sortOrder;
	};

	public getPageSize(): number {
		return this._pageSize;
	};

	public getPageCount(): number {
		return Math.ceil( this.data.length / this._pageSize );
	};

	private _slice() {
		if ( this._page > this.getPageCount() ) {
			this._page = this.getPageCount();
		}
		let offset: number = ( this._page - 1 ) * this._pageSize;
		this.page = this.data.slice( offset, offset + this._pageSize );
	};

}
