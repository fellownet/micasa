import {
	Directive,
	Input,
	OnInit,
	OnChanges,
	SimpleChanges
}                 from '@angular/core';

@Directive( {
	selector: 'table[gridData]',
	exportAs: 'GridDirective'
} )

export class GridDirective implements OnInit, OnChanges {

	@Input( "gridData" ) public data: any[] = [];

	@Input( "gridSort" ) private _defaultSortField: string;
	@Input( "gridOrder" ) private _defaultSortOrder: string;
	@Input( "gridPageSize" ) private _pageSize: number = 15;

	private _sortField: string = '';
	private _sortOrder: string = 'asc';
	private _page: number = 1;

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
			this._sort();
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
			this._sort();
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

	private _sort() {
		var me = this;
		this.data.sort( function( a_: any, b_: any ): number {
			let a: any = ( typeof( a_[me._sortField] ) == 'string' ? a_[me._sortField].toUpperCase() : a_[me._sortField] );
			let b: any = ( typeof( b_[me._sortField] ) == 'string' ? b_[me._sortField].toUpperCase() : b_[me._sortField] );
			if ( a < b ) {
				return me._sortOrder == 'asc' ? -1 : 1;
			} else if ( a > b ) {
				return me._sortOrder == 'asc' ? 1 : -1;
			} else {
				return 0;
			}
		} );
	};

	private _slice() {
		let offset: number = ( this._page - 1 ) * this._pageSize;
		this.page = this.data.slice( offset, offset + this._pageSize );
	};

}
