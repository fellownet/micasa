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
				// passed to the data fetchers which in turn will not fetch these devices again.
				return this._screensService.getDevicesOnScreen( screen_ )
					// Catch 404 errors if a device was removed but is still present in one of the widgets.
					.catch( error_ => {
						if ( error_.code == 404 ) {
							return Observable.of( [] );
						} else {
							return Observable.throw( error_ );
						}
					} )
					.mergeMap( devices_ => {
						let observables: Observable<[number,SourceData[]]>[]= [];
						screen_.widgets.forEach( ( widget_: Widget, i_: number ) => {
							observables.push(
								this._screensService.getDataForWidget( widget_, devices_ )
									.map( data_ => [ i_, data_ ] )
									// NOTE catch all 404 not found errors to make sure the forkJoin completes if a
									// device was deleted.
									.catch( error_ => {
										if ( error_.code == 404 ) {
											return Observable.of( null );
										} else {
											return Observable.throw( error_ );
										}
									 } )
							);
						} );
						if ( observables.length > 0 ) {
							return Observable.forkJoin( observables, function( ... args ) {
								let result: SourceData[][] = [];
								for ( let data of args as [number, SourceData[]][] ) {
									if ( !! data ) { // filters out null's (see catch notes)
										result[data[0]] = data[1];
									}
								}
								return { screen: screen_, data: result };
							} );
						} else {
							return Observable.of( { screen: screen_, data: [] } );
						}
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
