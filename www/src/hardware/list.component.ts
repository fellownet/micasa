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
		this._route.data
			.subscribe(
				data_ => {
					this.hardware = data_.list;

					if ( !! this.parent ) {
						this.startPage = this._hardwareService.lastPage[this.parent.id] || 1;
					} else {
						this.startPage = this._hardwareService.lastPage['global'] || 1;
					}
				}
			)
		;
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			if ( !! this.parent ) {
				this._hardwareService.lastPage[this.parent.id] = this._paging.getActivePage();
			} else {
				this._hardwareService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectHardware( hardware_: Hardware ) {
		this._hardwareService.returnUrl = this._router.url;
		this._router.navigate( [ '/hardware', hardware_.id, 'edit' ] );
	};

	public addHardware( type_: string ) {
		this._hardwareService.addHardware( type_ )
			.subscribe(
				hardware_ => {
					this._hardwareService.returnUrl = this._router.url;
					this._router.navigate( [ '/hardware', hardware_.id, 'edit' ] );
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
