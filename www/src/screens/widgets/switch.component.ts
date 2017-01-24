import { Component, Input }       from '@angular/core';
import { Widget }                   from '../screens.service';
import { Device, DevicesService } from '../../devices/devices.service';

@Component( {
	selector: 'switchwidget',
	templateUrl: 'tpl/widgets/switch.html'
} )

export class WidgetSwitchComponent {

	@Input( 'widgetConfig' ) public widget: Widget;

	constructor(
		private _devicesService: DevicesService
	) {
	};

	toggleSwitch() {
		// TODO support for other types of switches (blinds etc).
		var me = this;
		if ( me.widget.device.value == 'On' ) {
			me.widget.device.value = 'Off';
		} else {
			me.widget.device.value = 'On';
		}
		this._devicesService.patchDevice( me.widget.device, me.widget.device.value )
			.subscribe(
				function( device_: Device ) {
				},
				function( error_: string ) {
				}
			)
		;
	};

}
