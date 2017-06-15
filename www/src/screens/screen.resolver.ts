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
		let observable: Observable<Screen>;
		if ( route_.data['dashboard'] ) {
			observable = this._screensService.getScreen( 1 );
		} else if ( 'device_id' in route_.params  ) {
			observable = this._devicesService.getDevice( +route_.params['device_id'] )
				.map( device_ => this._screensService.getDefaultScreenForDevice( device_ ) )
			;
		} else if ( ! ( 'screen_id' in route_.params ) ) {
			observable = Observable.of( { id: NaN, name: 'New screen', widgets: [] } );
		} else {
			observable = this._screensService.getScreen( +route_.params['screen_id'] );
		}

		return observable
			.mergeMap( screen_ => {

				// First fetch *all* the devices that are used *anywhere* on the screen. This list of devices is then
				// passed to the data fetchers.
				return this._screensService.getDevicesOnScreen( screen_ )
					.mergeMap( devices_ => {
						let observables: Observable<[number,SourceData[]]>[]= [];
						screen_.widgets.forEach( ( widget_: Widget, i_: number ) => {
							observables.push(
								this._screensService.getDataForWidget( widget_, devices_ )
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
						let devicesObservable: Observable<Device[]> = this._devicesService.getDevices( { light: 1 } );
						return Observable.forkJoin( dataObservable, devicesObservable, function( data_: SourceData[][], devices_: Device[] ) {
							return { screen: screen_, data: data_, devices: devices_ };
						} );
					} )
				;
			} )
			.catch( () => {
				this._router.navigate( [ '/error' ] );
				return Observable.of( null );
			} )
		;
	};

}
