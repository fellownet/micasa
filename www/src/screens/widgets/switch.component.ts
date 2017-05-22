import {
	Component,
	Input,
	OnInit
}                       from '@angular/core';

import {
	WidgetComponent
}                       from '../widget.component';
import {
	Device,
	DevicesService
}                       from '../../devices/devices.service'

@Component( {
	selector: 'switchwidget',
	templateUrl: 'tpl/widgets/switch.html'
} )

export class WidgetSwitchComponent implements OnInit {

	public editing: boolean = false;

	@Input( 'widget' ) public parent: WidgetComponent;
	@Input( 'placeholder' ) public placeholder: boolean;

	public constructor(
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
	};

	public toggle() {
		var me = this;
		if ( me.parent.devices[me.parent.widget.sources[0].device_id].value == 'On' ) {
			me.parent.devices[me.parent.widget.sources[0].device_id].value = 'Off';
		} else {
			me.parent.devices[me.parent.widget.sources[0].device_id].value = 'On';
		}
		me._devicesService.patchDevice( me.parent.devices[me.parent.widget.sources[0].device_id], me.parent.devices[me.parent.widget.sources[0].device_id].value )
			.subscribe(
				function( device_: Device ) {
				},
				function( error_: string ) {
				}
			)
		;
	};

}
