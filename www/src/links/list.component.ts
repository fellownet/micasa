import {
	Component,
	OnInit,
	OnDestroy,
	Input,
	ViewChild
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import {
	Device
}                  from '../devices/devices.service';
import {
	Link,
	LinksService
}                  from './links.service'
import {
	GridPagingComponent
}                  from '../grid/paging.component'

@Component( {
	selector: 'links',
	templateUrl: 'tpl/links-list.html'
} )

export class LinksListComponent implements OnInit, OnDestroy {

	public error: String;
	public links: Link[];
	public startPage: number = 1;

	@Input() public device?: Device;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;
	
	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _linksService: LinksService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.links = data_.links;
			if ( !!me.device ) {
				me.startPage = me._linksService.lastPage[me.device.id] || 1;
			} else {
				me.startPage = me._linksService.lastPage['global'] || 1;
			}
		} );
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			if ( !!this.device ) {
				this._linksService.lastPage[this.device.id] = this._paging.getActivePage();
			} else {
				this._linksService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectLink( link_: Link ) {
		this._linksService.returnUrl = this._router.url;
		if ( !!this.device ) {
			this._router.navigate( [ 'links', link_.id, 'device', this.device.id ] );
		} else {
			this._router.navigate( [ '/links', link_.id ] );
		}
	};

	public addLink() {
		this._linksService.returnUrl = this._router.url;
		if ( !!this.device ) {
			this._router.navigate( [ '/links', 'add', 'device', this.device.id ] );
		} else {
			this._router.navigate( [ '/links', 'add' ] );
		}
	};
}
