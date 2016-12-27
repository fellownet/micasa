import { Component, Input } from '@angular/core';
import { Device }           from '../devices/devices.service';

@Component( {
	selector: 'textwidget',
	templateUrl: 'tpl/widgets/text.html'
} )

export class WidgetTextComponent {

	@Input( "device" ) public device: Device;

}
