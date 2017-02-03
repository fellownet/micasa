import {
	Component,
	OnInit,
	Input
}                   from '@angular/core';
import {
	Router,
	ActivatedRoute
}                   from '@angular/router';

import {
	Hardware,
	HardwareService
}                   from './hardware.service';

@Component( {
	selector: 'hardware',
	templateUrl: 'tpl/hardware-list.html',
} )

export class HardwareListComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public hardware: Hardware[];

	@Input() public parent?: Hardware;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _hardwareService: HardwareService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.hardware = data_.list;
		} );
	};

	public selectHardware( hardware_: Hardware ) {
		this.loading = true;
		if ( this.parent ) {
			this._router.navigate( [ '/hardware', this.parent.id, hardware_.id, 'edit' ] );
		} else {
			this._router.navigate( [ '/hardware', hardware_.id, 'edit' ] );
		}
	};

	public addHardware( type_: string ) {
		var me = this;
		me.loading = true;
		this._hardwareService.addHardware( type_ )
			.subscribe(
				function( hardware_: Hardware ) {
					me.loading = false;
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
