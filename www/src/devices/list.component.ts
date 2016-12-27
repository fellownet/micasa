import { Component, OnInit, Input } from '@angular/core';
import { Router }                   from '@angular/router';
import { Device, DevicesService }   from './devices.service';
import { Hardware }                 from '../hardware/hardware.service';

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices-list.html'
} )

export class DevicesListComponent implements OnInit {

	loading: Boolean = false;
	error: String;
	devices: Device[] = [];
	@Input() hardware: Hardware; // gets set when used from the hardware details component

	constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	ngOnInit() {
		this.getDevices();
	};

	getDevices() {
		var me = this;
		me._devicesService.getDevices( me.hardware )
			.subscribe(
				function( devices_: Device[] ) {
					me.devices = devices_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	selectDevice( device_: Device ) {
		if ( this.hardware ) {
			this._router.navigate( [ '/hardware', this.hardware.id, device_.id ] );
		} else {
			this._router.navigate( [ '/devices', device_.id ] );
		}
	};

}
