import { Component, Input }       from '@angular/core';
import { Device, DevicesService } from '../devices/devices.service';

@Component( {
	selector: 'switchwidget',
	templateUrl: 'tpl/widgets/switch.html'
} )

export class WidgetSwitchComponent {

	@Input( "device" ) public device: Device;

	constructor(
		private _devicesService: DevicesService
	) {
	};



	toggleSwitch() {
		// TODO support for other types of switches (blinds etc).
		var me = this;
		if ( me.device.value == 'On' ) {
			me.device.value = 'Off';
		} else {
			me.device.value = 'On';
		}
		this._devicesService.putDevice( me.device, true )
			.subscribe(
				function( device_: Device ) {
				},
				function( error_: string ) {
				}
			)
		;
	};

}
