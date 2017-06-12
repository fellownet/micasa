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
	Widget,
	Source,
	SourceData,
	Screen
}                         from './screens.service';
import {
	Device
}                         from '../devices/devices.service';

@Component( {
	templateUrl: 'tpl/screen.html'
} )

export class ScreenComponent implements OnInit {

	public screen: Screen;
	public devices: Device[];
	public data: SourceData[][];

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		var me = this;

		this._route.data.subscribe( function( data_: any ) {

			me.screen = data_.payload.screen;
			me.data = data_.payload.data;
			me.devices = data_.payload.devices;




/*

			// Let's do some housekeeping and remove sources without valid devices and widgets without valid sources.
			// NOTE To be able to remove items while iterating we're iterating backwards.
			for ( var i = me.screen.widgets.length; i--; ) {
				for ( var j = me.screen.widgets[i].sources.length; j--; ) {
					let source: Source = me.screen.widgets[i].sources[j];
					if ( ! me.devices[source.device_id] ) {
						me.screen.widgets[i].sources.splice( j, 1 );
					}
				}
				if ( me.screen.widgets[i].sources.length == 0 ) {
					me.screen.widgets.splice( i, 1 );
				}
			}
*/
		} );
	};

}
