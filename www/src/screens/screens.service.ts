import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
import {
	Device,
	DevicesService
}                         from '../devices/devices.service';

export class Source {
	device_id: number;
	properties: any;
};

export class SourceData {
	device: Device;
	data: any;
};

export class Widget {
	name: string;
	size: string;
	type: string;
	properties: any;
	sources: Source[];
	interval?: string;
	range?: number;
};

export class Screen {
	id: number;
	name: string;
	widgets: Widget[];
};

@Injectable()
export class ScreensService {

	public constructor(
		private _sessionService: SessionService,
		private _devicesService: DevicesService,
	) {
	};

	public getScreens(): Observable<Screen[]> {
		return this._sessionService.retrieve<Screen[]>( 'screens' );
	};

	public getScreen( id_: number ): Observable<Screen> {
		return this.getScreens()
			.map( function( screens_: Screen[] ) {
				for ( let screen of screens_ ) {
					if ( screen.id == id_ ) {
						// Make a deep clone. NOTE this is a rather ugly way of cloning, replace with a better one if
						// available. However, json will work because the screen only contains literals and needs to
						// be jsonified anyhow when being pushed to the server.
						return JSON.parse( JSON.stringify( screen ) );
					}
				}
				throw new Error( 'invalid screen' );
			} )
		;
	};

	public putScreen( screen_: Screen ): Observable<Screen> {
		var me = this;
		return me.getScreens()
			.mergeMap( function( screens_: Screen[] ) {
				let id: number = 0;
				let updated: boolean = false;
				for ( let i: number = 0; i < screens_.length; i++ ) {
					id = Math.max( id, screens_[i].id );
					if ( screens_[i].id == screen_.id ) {
						screens_[i] = screen_;
						updated = true;
					}
				}
				if ( ! updated ) {
					screen_.id = ++id;
					screens_.push( screen_ );
				}
				return me._sessionService.store( 'screens', screens_ );
			} )
			.map( () => screen_ )
		;
	};

	public deleteScreen( screen_: Screen ): Observable<Screen[]> {
		var me = this;
		return me.getScreens()
			.mergeMap( function( screens_: Screen[] ) {
				for ( let i: number = 0; i < screens_.length; i++ ) {
					if ( screens_[i].id == screen_.id ) {
						screens_.splice( i, 1 );
						break;
					}
				}
				return me._sessionService.store( 'screens', screens_ );
			} )
		;
	};

	public getDevicesOnScreen( screen_: Screen ): Observable<Device[]> {
		let device_ids: number[] = [];
		for ( let widget of screen_.widgets ) {
			for ( let source of widget.sources ) {
				if ( device_ids.indexOf( source.device_id ) > -1 ) {
					continue;
				}
				device_ids.push( source.device_id );
			}
		}
		if ( device_ids.length == 0 ) {
			return Observable.of( [] );
		} else {
			return this._devicesService.getDevicesByIds( device_ids )
				.map( function( devices_: Device[] ) {
					let result: Device[] = [];
					for ( let device of devices_ ) {
						result[device.id] = device;
					}
					return result;
				} )
			;
		}
	};

	public getDataForWidget( widget_: Widget, devices_?: Device[] ): Observable<SourceData[]> {

		// First all the devices that are used for the widget sources are fetched. If such a device is already present
		// in the optionally provided devices array it is not fetched again.
		let me: ScreensService = this;
		let observables: Observable<Device>[] = [];
		for ( let source of widget_.sources ) {
			if (
				!! devices_
				&& !! devices_[source.device_id]
			) {
				observables.push( Observable.of( devices_[source.device_id] ) );
			} else {
				observables.push(
					this._devicesService.getDevice( source.device_id )
						// NOTE catch all errors to make sure the forkJoin completes.
						.catch( () => Observable.of( null ) )
				);
			}
		}

		return Observable.forkJoin( observables, function( ... args ) {
			let result: Device[] = [];
			for ( let device of args as Device[] ) {
				if ( !! device ) { // filters out null's (see .catch above)
					result[device.id] = device;
				}
			}
			return result;
		} ).mergeMap( function( devices_: Device[] ) {

			// At this point we shoud have all the devices required for all the sources of the widget. The devices are
			// stored as device_id => device in the devices array.
			let observables: Observable<[number, Device, any]>[] = [];
			widget_.sources.forEach( function( source_: Source, i_: number ) {

				// Depending on the type of the widget, the data observables are gathered.
				let device: Device = devices_[source_.device_id];
				switch( widget_.type ) {
					case 'chart':

						// The data for a chart widget depends on the type of device that is providing the data.
						switch( device.type ) {
							case 'switch':
							case 'text':
								observables.push(
									me._devicesService.getData( device.id, {
										range: widget_.range,
										interval: widget_.interval
									} )
									.map( data_ => [ i_, device, data_ ] )
									// NOTE catch all errors to make sure the forkJoin completes.
									.catch( () => Observable.of( null ) )
								);
								break;
							
							case 'level':
								observables.push(
									me._devicesService.getData( device.id, {
										group:
											[ '5min', '5min', 'hour', 'day', 'day' ][
												[ 'hour', 'day', 'week', 'month', 'year' ].indexOf( widget_.interval )
											],
										range: widget_.range,
										interval: widget_.interval
									} )
									.map( data_ => [ i_, device, data_ ] )
									// NOTE catch all errors to make sure the forkJoin completes.
									.catch( () => Observable.of( null ) )
								);
								break;

							case 'counter':
								observables.push(
									me._devicesService.getData( device.id, {
										group:
											[ 'hour', 'hour', 'day', 'day', 'day' ][
												[ 'hour', 'day', 'week', 'month', 'year' ].indexOf( widget_.interval )
											],
										range: widget_.range,
										interval: widget_.interval
									} )
									.map( data_ => [ i_, device, data_ ] )
									// NOTE catch all errors to make sure the forkJoin completes.
									.catch( () => Observable.of( null ) )
								);
								break;
						}
						break;

					case 'gauge':
						observables.push(
							me._devicesService.getData( device.id, {
								group: '5min',
								range: 1,
								interval: 'day'
							} )
							.map( data_ => [ i_, device, data_ ] )
							// NOTE catch all errors to make sure the forkJoin completes.
							.catch( () => Observable.of( null ) )
						);
						break;

					case 'table':
						observables.push(
							me._devicesService.getData( device.id, {
								range: 1,
								interval: 'week'
							} )
							.map( data_ => [ i_, device, data_ ] )
							// NOTE catch all errors to make sure the forkJoin completes.
							.catch( () => Observable.of( null ) )
						);
						break;
					
					default:
						observables.push( Observable.of( [ i_, device, [] ] ) );
						break;
				}
			} );

			return Observable.forkJoin( observables, function( ... args ) {
				let result: SourceData[] = [];
				for ( let data of args as [number, Device, any][] ) {
					if ( !! data ) { // filters out null's (see catch notes)
						result[data[0]] = { device: data[1], data: data[2] };
					}
				}
				return result;
			} );
		} );




		// TODO
		// see if the device is in devices_ and use it instead of fetching a new one
		// get the data for the widget and add it to the widget along with the device.
		// the data fetches from each individual component (none for latest or switch) should be here
		// in the screen.tpl pass the data instead of the widgets, change the components accordingly.
		// on reload, get new data and update the data property of the widget which will trigger ngChanges in the
		// widgets.

		// optionally remove the widget input properties that aren't needed



	};

	public getDefaultScreenForDevice( device_: Device ) {
		let screen: Screen;
		switch( device_.type ) {
			case 'level':
			case 'counter':
				screen = {
					id: NaN,
					name: device_.name,
					widgets: [ {
						type: 'chart',
						name: 'Day',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue'
							}
						} ],
						interval: 'day',
						range: 1
					}, {
						type: 'chart',
						name: 'Week',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue'
							}
						} ],
						interval: 'week',
						range: 1
					}, {
						type: 'chart',
						name: 'Month',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue',
								range: [ 'temperature', 'humidity' ].indexOf( device_.subtype ) > -1,
								trendline: [ 'temperature', 'power', 'energy', 'gas', 'water' ].indexOf( device_.subtype ) > -1,
								trendline_color: 'red'
							}
						} ],
						interval: 'month',
						range: 1
					}, {
						type: 'chart',
						name: 'Year',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue',
								range: [ 'temperature', 'humidity' ].indexOf( device_.subtype ) > -1,
								trendline: [ 'temperature', 'power', 'energy', 'gas', 'water' ].indexOf( device_.subtype ) > -1,
								trendline_color: 'red'
							}
						} ],
						interval: 'year',
						range: 1
					} ]
				};
				break;

			case 'switch':
			case 'text':
				screen = {
					id: NaN,
					name: device_.name,
					widgets: [ {
						type: 'table',
						name: 'History',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {}
						} ],
						interval: 'week',
						range: 1
					} ]
				};
				break;

		}
		return screen;
	};

};
