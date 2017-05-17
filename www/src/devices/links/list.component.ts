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
	Device,
	DevicesService,
	Link
}                  from '../devices.service';
import {
	GridPagingComponent
}                  from '../../grid/paging.component'

@Component( {
	selector: 'links',
	templateUrl: 'tpl/links-list.html'
} )

export class LinksListComponent implements OnInit, OnDestroy {

	public loading: boolean = false;
	public error: String;
	public links: Link[];
	public startPage: number = 1;

	@Input() public device: Device;
	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;
	
	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data
			.subscribe( function( data_: any ) {
				me.links = data_.links;
				me.startPage = me._devicesService.lastPage[me.device.id] || 1;
			} )
		;
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			this._devicesService.lastPage[this.device.id] = this._paging.getActivePage();
		}
	};

	public selectLink( link_: Link ) {
		this.loading = true;
		this._router.navigate( [ '/devices', link_.device_id, 'links', link_.id ] );
	};

	public addLink() {
		this.loading = true;
		this._router.navigate( [ '/devices', this.device.id, 'links', 'add' ] );
	};
}
