import { Component, OnInit }               from '@angular/core';
import { Input, OnChanges, SimpleChanges } from '@angular/core';
import { Router }                          from '@angular/router';
import { Device, DevicesService }          from './devices.service';
import { Hardware }                        from '../hardware/hardware.service';

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices-list.html'
} )

export class DevicesListComponent implements OnInit, OnChanges {

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

	ngOnChanges( changes_: SimpleChanges ) {
		this.getDevices();
	};

	getDevices() {
		var me = this;
		me.loading = true;
		me._devicesService.getDevices( me.hardware )
			.subscribe(
				function( devices_: Device[] ) {
					me.loading = false;
					me.devices = devices_;
				},
				function( error_: String ) {
					me.loading = false;
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
