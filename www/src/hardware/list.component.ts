import {
	Component,
	OnInit,
	OnDestroy,
	Input,
	ViewChild
}                   from '@angular/core';
import {
	Router,
	ActivatedRoute
}                   from '@angular/router';

import {
	Hardware,
	HardwareService
}                   from './hardware.service';
import {
	GridPagingComponent
}                   from '../grid/paging.component'

@Component( {
	selector: 'hardware',
	templateUrl: 'tpl/hardware-list.html',
} )

export class HardwareListComponent implements OnInit, OnDestroy {

	public loading: boolean = false;
	public error: String;
	public hardware: Hardware[];
	public startPage: number = 1;

	@Input() public parent?: Hardware;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _hardwareService: HardwareService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.loading = false;
			me.hardware = data_.list;
			if ( !!me.parent ) {
				me.startPage = me._hardwareService.lastPage[me.parent.id] || 1;
			} else {
				me.startPage = me._hardwareService.lastPage['global'] || 1;
			}
		} );
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			if ( !!this.parent ) {
				this._hardwareService.lastPage[this.parent.id] = this._paging.getActivePage();
			} else {
				this._hardwareService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectHardware( hardware_: Hardware ) {
		this.loading = true;
		this._hardwareService.returnUrl = this._router.url;
		this._router.navigate( [ '/hardware', hardware_.id, 'edit' ] );
	};

	public addHardware( type_: string ) {
		var me = this;
		me.loading = true;
		this._hardwareService.addHardware( type_ )
			.subscribe(
				function( hardware_: Hardware ) {
					me._hardwareService.returnUrl = this._router.url;
					me._router.navigate( [ '/hardware', hardware_.id, 'edit' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

}
