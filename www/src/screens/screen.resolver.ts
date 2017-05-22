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
		var me = this;

		return Observable.create( function( observer_: Observer<string> ) {
			let result: any = {};

			if ( route_.data['dashboard'] ) {

				me._screensService.getScreen( 1 )
					.subscribe( function( screen_: Screen ) {
						result.screen = screen_;
						observer_.next( result );
						observer_.complete();
					}, function( error_: string ) {
						me._router.navigate( [ '/login' ] );
					} )
				;

			} else if ( 'device_id' in route_.params  ) {

				me._devicesService.getDevice( +route_.params['device_id'] )
					.subscribe( function( device_: Device ) {
						result.device = device_;
						me._screensService.getScreenForDevice( device_.id )
							.subscribe( function( screen_: Screen ) {
								result.screen = screen_;
								observer_.next( result );
								observer_.complete();
							}, function( error_: string ) {
								result.screen = me._screensService.getDefaultScreenForDevice( device_ );
								me._screensService.putScreen( result.screen ).subscribe();
								observer_.next( result );
								observer_.complete();
							} )
						;
					}, function( error_: string ) {
						me._router.navigate( [ '/login' ] );
					} )
				;

			} else if ( ! ( 'screen_id' in route_.params ) ) {

				result.screen = { id: NaN, name: 'New screen', widgets: [] };
				observer_.next( result );
				observer_.complete();

			} else {

				me._screensService.getScreen( +route_.params['screen_id'] )
					.subscribe( function( screen_: Screen ) {
						result.screen = screen_;
						if ( !!result.screen.device_id ) {
							me._devicesService.getDevice( result.screen.device_id )
								.subscribe( function( device_: Device ) {
									result.device = device_;
									observer_.next( result );
									observer_.complete();
								}, function( error_: string ) {
									me._router.navigate( [ '/login' ] );
								} )
							;
						} else {
							observer_.next( result );
							observer_.complete();
						}
					}, function( error_: string ) {
						me._router.navigate( [ '/login' ] );
					} )
				;
			}
		} );
	};
}
