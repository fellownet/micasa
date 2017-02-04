import {
	Component,
	Input,
	OnInit
}                  from '@angular/core';

import {
	Screen,
	Widget
}                  from '../screens.service';
import {
	Device,
	DevicesService
}                  from '../../devices/devices.service';

@Component( {
	selector: 'switchwidget',
	templateUrl: 'tpl/widgets/switch.html'
} )

export class WidgetSwitchComponent {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'device' ) public device: Device;

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
		this._devicesService.patchDevice( me.device, me.device.value )
			.subscribe(
				function( device_: Device ) {
				},
				function( error_: string ) {
				}
			)
		;
	};

}
