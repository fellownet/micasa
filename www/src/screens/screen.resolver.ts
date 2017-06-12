import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import { Observer }        from 'rxjs/Observer';

import {
	Screen,
	Widget,
	SourceData,
	ScreensService,
}                          from './screens.service';
import {
	Device,
	DevicesService
}                          from '../devices/devices.service';

@Injectable()
export class ScreenResolver implements Resolve<Screen> {

	public constructor(
		private _router: Router,
		private _screensService: ScreensService,
		private _devicesService: DevicesService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<any> {
		let me: ScreenResolver = this;

		let observable: Observable<Screen>;
		if ( route_.data['dashboard'] ) {
			observable = me._screensService.getScreen( 1 );
		} else if ( 'device_id' in route_.params  ) {
			observable = me._devicesService.getDevice( +route_.params['device_id'] )
				.map( device_ => me._screensService.getDefaultScreenForDevice( device_ ) )
			;
		} else if ( ! ( 'screen_id' in route_.params ) ) {
			observable = Observable.of( { id: NaN, name: 'New screen', widgets: [] } );
		} else {
			observable = me._screensService.getScreen( +route_.params['screen_id'] );
		}

		return observable
			.mergeMap( function( screen_: Screen ) {

				// First fetch *all* the devices that are used *anywhere* on the screen. This list of devices is then
				// passed to the data fetchers.
				return me._screensService.getDevicesOnScreen( screen_ )
					.mergeMap( function( devices_: Device[] ) {

						let observables: Observable<[number,SourceData[]]>[]= [];
						screen_.widgets.forEach( function( widget_: Widget, i_: number ) {
							observables.push(
								me._screensService.getDataForWidget( widget_, devices_ )
								.map( data_ => [ i_, data_ ] )
								// NOTE catch all errors to make sure the forkJoin completes.
								.catch( () => Observable.of( null ) )
							);
						} );

						let dataObservable: Observable<SourceData[][]>;
						if ( observables.length > 0 ) {
							dataObservable = Observable.forkJoin( observables, function( ... args ) {
								let result: SourceData[][] = [];
								for ( let data of args as [number, SourceData[]][] ) {
									if ( !! data ) { // filters out null's (see catch notes)
										result[data[0]] = data[1];
									}
								}
								return result;
							} );
						} else {
							dataObservable = Observable.of( [] );
						}
						let devicesObservable: Observable<Device[]> = me._devicesService.getDevices();
						return Observable.forkJoin( dataObservable, devicesObservable, function( data_: SourceData[][], devices_: Device[] ) {
							return { screen: screen_, data: data_, devices: devices_ };
						} );
					} )
				;












/*
				// DEBUG
				setTimeout( function() {
					let poe: Observable<Widget>[] = [];
					for ( let widget of screen_.widgets ) {
						poe.push( me._screensService.getDataForWidget( widget ) );
					}
					Observable.forkJoin( poe, function( ... args ) { console.log( args ); } ).subscribe();
				}, 100 );
*/
/*
				let result: any = { screen: screen_ };
				return me._screensService.getDevicesOnScreen( screen_ )
					.map( function( devices_: Device[] ) {
						result.devices = devices_;
						return result;
					} )
				;
*/
			} )
			.catch( () => me._router.navigate( [ '/login' ] ) )
		;
	};
}
