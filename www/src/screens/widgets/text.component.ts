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
	selector: 'textwidget',
	templateUrl: 'tpl/widgets/text.html'
} )

export class WidgetTextComponent {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'device' ) public device: Device;

	public delete() {
		alert( 'delete' );
	};

	public edit() {
		alert( 'edit' );
	};

}
