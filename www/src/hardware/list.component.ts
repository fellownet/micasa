import { Component, OnInit }         from '@angular/core';
import { Router }                    from '@angular/router';
import { Hardware, HardwareService } from './hardware.service';

declare var $: any;

@Component( {
	selector: 'hardware',
	templateUrl: 'tpl/hardware-list.html',
} )

export class HardwareListComponent implements OnInit {

	loading: Boolean = false;
	error: String;
	hardware: Hardware[] = [];

	constructor(
		private _router: Router,
		private _hardwareService: HardwareService
	) {
	};

	ngOnInit() {
		this.getHardware();
	};

	getHardware() {
		var me = this;
		this._hardwareService.getHardwareList()
			.subscribe(
				function( hardware_: Hardware[] ) {
					me.hardware = hardware_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	selectHardware( hardware_: Hardware ) {
		this._router.navigate( [ '/hardware', hardware_.id ] );
	};

}
