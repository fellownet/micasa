import { Component, Input } from '@angular/core';
import { Widget }                   from '../screens.service';
import { Device }           from '../../devices/devices.service';

@Component( {
	selector: 'textwidget',
	templateUrl: 'tpl/widgets/text.html'
} )

export class WidgetTextComponent {

	@Input( 'widgetConfig' ) public widget: Widget;

}
