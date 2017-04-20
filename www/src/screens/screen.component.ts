import {
	Component,
	OnInit,
	OnDestroy
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
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

export class ScreenComponent implements OnInit, OnDestroy {

	private _active: boolean = true;

	public loading: boolean = false;
	public error: string;
	public screen: Screen;
	public devices: Device[] = [];

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _sessionService: SessionService,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.screen;

			let observables: Observable<Device>[] = [];
			for ( let widget of me.screen.widgets ) {
				observables.push( me._devicesService.getDevice( widget.device_id ) );
			}
			Observable
				.forkJoin( observables )
				.subscribe( function( devices_: Device[] ) {
					me.devices = devices_;
				} )
			;
		} );

		this._sessionService.events
			.takeWhile( () => this._active )
			.subscribe( function( event_: any ) {
				console.log( 'got an event in screen component' );
				console.log( event_ );
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
	};

}
