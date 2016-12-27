import { Component, Input } from '@angular/core';
import { Device }           from '../devices/devices.service';

@Component( {
	selector: 'switchwidget',
	templateUrl: 'tpl/widgets/switch.html'
} )

export class WidgetSwitchComponent {

	@Input( "device" ) public device: Device;

}
