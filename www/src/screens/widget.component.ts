import {
	Component,
	Input,
	OnInit,
	OnDestroy
}                         from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import {
	Screen,
	ScreensService,
	Widget
}                         from './screens.service';
import {
	Device,
	DevicesService
}                         from '../devices/devices.service'
import { SessionService } from '../session/session.service';

@Component( {
	selector: 'widget',
	templateUrl: 'tpl/widget.html'
} )

export class WidgetComponent implements OnInit, OnDestroy {

	public loading: boolean = false;
	public error?: string;

	@Input( 'screen' ) public screen: Screen;
	@Input( 'device' ) public device?: Device;
	@Input( 'widget' ) public widget: Widget;

	public devices: any; // key = device_id, value = device
	public data: any; // key = device_id, value = data array

	private _active: boolean = true;

	public constructor(
		private _screensService: ScreensService,
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;
		me._load().subscribe( function() {

			// Listen for events broadcasted from the session service.
			me._sessionService.events
				.takeWhile( () => me._active )
				.subscribe( function( event_: any ) {
					console.log( 'got an event in screen component' );
					console.log( event_ );

					// TODO see if the event targets one of our devices.

				} )
			;
		} );
	};

	public ngOnDestroy() {
		this._active = false;
	};

	private _load(): Observable<null> {
		var me = this;
		me.loading = true;

		me.devices = {};
		me.data = {};

		// First all devices that are related to this widget are fetched. If a device has already been fetched, it is
		// not fetched again.
		let observables: Observable<Device>[] = [];
		let done: number[] = [];
		for ( let source of me.widget.sources ) {
			if ( done.indexOf( source.device_id ) > -1 ) {
				continue;
			}
			if ( !!me.device && me.device.id == source.device_id ) {
				observables.push( Observable.of( me.device ) );
			} else {
				observables.push(
					me._devicesService.getDevice( source.device_id )

						// NOTE we're capturing http errors when the device cannot be fetched. If we wouldn't, the
						// forkJoin would never complete and the widget becomes unusable. Instead, the faield source is
						// removed from the widget.
						.catch( function( error_: string ) {
							let index: number = me.widget.sources.indexOf( source );
							if ( index > -1 ) {
								me.widget.sources.splice( index, 1 );
							}
							return Observable.of( null );
						} )
				);
			}
			done.push( source.device_id );
		}
		return Observable
			.forkJoin( observables )
			.mergeMap( function( devices_: Device[] ) {

				// Then for each fetched device, the data is fetched using the date range of the widget.
				let observables: Observable<[number,any[]]>[] = [];
				for ( let device of devices_ ) {
					if ( null != device ) { // in case of an error
						me.devices[device.id] = device;
						switch( device.type ) {
							case 'switch':
							case 'text':
							default:
								observables.push(
									me._devicesService.getData( device.id, {
										range: me.widget.range,
										interval: me.widget.interval
									} )
									.map( function( data_: any ) {
										return [ device.id, data_ ];
									} )
								);
								break;
							
							case 'level':
								observables.push(
									me._devicesService.getData( device.id, {
										group:
											[ '5min', '5min', 'day', 'day', 'day' ][
												[ 'hour', 'day', 'week', 'month', 'year' ].indexOf( me.widget.interval )
											],
										range: me.widget.range,
										interval: me.widget.interval
									} )
									.map( function( data_: any ) {
										return [ device.id, data_ ];
									} )
								);
								break;

							case 'counter':
								observables.push(
									me._devicesService.getData( device.id, {
										group:
											[ 'hour', 'hour', 'day', 'day', 'day' ][
												[ 'hour', 'day', 'week', 'month', 'year' ].indexOf( me.widget.interval )
											],
										range: me.widget.range,
										interval: me.widget.interval
									} )
									.map( function( data_: any ) {
										return [ device.id, data_ ];
									} )
								);
								break;
						}
					}
				}

				// TODO if there are no observables then all the devices of this widget have been removed and the
				// widget needs to be removed.

				return Observable
					.forkJoin( observables )
					.mergeMap( function( data_: any[] ) {
						for ( let data of data_ ) {
							me.data[data[0]] = data[1];
						}
						me.loading = false;
						return Observable.of( null );
					} )
				;
			} )
		;
	};

	public get self(): WidgetComponent { // allows to pass the component to the child through input binding
		return this;
	};

	public get classes(): any {
		return {
			'col-xs-12': true,
			'col-sm-12': true,
			'col-md-12': this.widget.size == 'large',
			'col-md-6': this.widget.size == 'medium' || this.widget.size == 'small',
			'col-lg-12': this.widget.size == 'large',
			'col-lg-6': this.widget.size == 'medium',
			'col-lg-4': this.widget.size == 'small'
		};
	};

	public delete() {
		let index = this.screen.widgets.indexOf( this.widget );
		if ( index > -1 ) {
			this.screen.widgets.splice( index, 1 );
			this._screensService.putScreen( this.screen ).subscribe();
		}
	};

	public save( reload_: boolean = true ) {
		this._screensService.putScreen( this.screen ).subscribe();
		if ( reload_ ) {
			this.reload();
		}
	};

	public reload() {
		this._load().subscribe();
	};

}
