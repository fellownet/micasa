import {
	Component,
	OnInit,
	ViewChild
}                               from '@angular/core';
import {
	Router,
	ActivatedRoute
}                               from '@angular/router';

import {
	Hardware,
	HardwareService
}                               from './hardware.service';
import {
	Device, DevicesService
}                               from '../devices/devices.service';
import { DevicesListComponent } from '../devices/list.component';

@Component( {
	templateUrl: 'tpl/hardware-edit.html'
} )

export class HardwareEditComponent implements OnInit {

	public error: String;
	public hardware: Hardware;
	public devices: Device[];
	public children: Hardware[];
	public title: string;

	public hasAdvancedSettings: boolean = false;
	public hasActionDevices: boolean = false;

	@ViewChild('devicesList') private _devicesListComponent: DevicesListComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _hardwareService: HardwareService,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.hardware = data_.hardware;
			me.devices = data_.devices;
			me.children = data_.list;
			me.title = me.hardware.name;
			for ( let device of me.devices ) {
				if ( device.subtype == 'action' ) {
					me.hasActionDevices = true;
				}
			}
			for ( let setting of me.hardware.settings ) {
				if ( setting.class == 'advanced' ) {
					me.hasAdvancedSettings = true;
				}
			}
		} );
	};

	public submitHardware() {
		var me = this;
		this._hardwareService.putHardware( me.hardware )
			.subscribe(
				function( hardware_: Hardware ) {
					if ( !!me._hardwareService.returnUrl ) {
						me._router.navigateByUrl( me._hardwareService.returnUrl );
						delete me._hardwareService.returnUrl
					} else {
						me._router.navigate( [ '/hardware' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
					window.scrollTo( 0, 0 );
				}
			)
		;
	};

	public deleteHardware() {
		var me = this;
		me._hardwareService.deleteHardware( me.hardware )
			.subscribe(
				function( hardware_: Hardware ) {
					if ( !!me._hardwareService.returnUrl ) {
						me._router.navigateByUrl( me._hardwareService.returnUrl );
						delete me._hardwareService.returnUrl
					} else {
						me._router.navigate( [ '/hardware' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	public performAction( action_: Device ) {
		var me = this;
		this._devicesService.performAction( action_ )
			.subscribe(
				function( device_: Device ) {
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};
}
