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
		this._route.data
			.subscribe(
				data_ => {
					this.hardware = data_.hardware;
					this.devices = data_.devices;
					this.children = data_.list;

					this.title = this.hardware.name;

					for ( let device of this.devices ) {
						if ( device.subtype == 'action' ) {
							this.hasActionDevices = true;
							break;
						}
					}
					for ( let setting of this.hardware.settings ) {
						if ( setting.class == 'advanced' ) {
							this.hasAdvancedSettings = true;
							break;
						}
					}
				}
			)
		;
	};

	public submitHardware() {
		this._hardwareService.putHardware( this.hardware )
			.subscribe(
				hardware_ => {
					if ( !! this._hardwareService.returnUrl ) {
						this._router.navigateByUrl( this._hardwareService.returnUrl );
						delete this._hardwareService.returnUrl
					} else {
						this._router.navigate( [ '/hardware' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteHardware() {
		this._hardwareService.deleteHardware( this.hardware )
			.subscribe(
				hardware_ => {
					if ( !! this._hardwareService.returnUrl ) {
						this._router.navigateByUrl( this._hardwareService.returnUrl );
						delete this._hardwareService.returnUrl
					} else {
						this._router.navigate( [ '/hardware' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public performAction( action_: Device ) {
		this._devicesService.performAction( action_ )
			.subscribe(
				() => undefined,
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
