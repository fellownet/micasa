import { Component, OnInit }         from '@angular/core';
import { Router, ActivatedRoute }    from '@angular/router';
import { Hardware, HardwareService } from './hardware.service';

@Component( {
	templateUrl: 'tpl/hardware-details.html',
} )

export class HardwareDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	hardware: Hardware;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _hardwareService: HardwareService
	) {
	};

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.hardware = data_.hardware;
		} );
	};

	submitHardware() {
		var me = this;
		me.loading = true;
		this._hardwareService.putHardware( me.hardware )
			.subscribe(
				function( hardware_: Hardware ) {
					me.loading = false;
					me._router.navigate( [ '/hardware' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	cancelEdit() {
		this._router.navigate( [ '/hardware' ] );
	};

}
