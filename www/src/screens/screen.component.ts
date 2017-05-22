import {
	Component,
	OnInit
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';
import { Observable }     from 'rxjs/Observable';

import {
	Device,
	DevicesService
}                         from '../devices/devices.service';
import {
	Widget,
	Screen
}                         from './screens.service';

@Component( {
	templateUrl: 'tpl/screen.html'
} )

export class ScreenComponent implements OnInit {

	public screen: Screen;
	public device?: Device;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;

		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.payload.screen;
			me.device = data_.payload.device;
		} );
	};

}
