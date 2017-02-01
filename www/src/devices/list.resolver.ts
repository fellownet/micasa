import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	Device,
	DevicesService
}                          from './devices.service';

@Injectable()
export class DevicesListResolver implements Resolve<Device[]> {

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Device[]> {
		var me = this;
		if ( route_.params['script_id'] == 'add' ) {
			return Observable.of( [] );
		} else {
			return new Observable( function( subscriber_: any ) {
				me._devicesService.getDevices( route_.params['hardware_id'], route_.params['script_id'] )
					.subscribe(
						function( devices_: Device[] ) {
							subscriber_.next( devices_ );
							subscriber_.complete();
						},
						function( error_: string ) {
							me._router.navigate( [ '/login' ] );
							subscriber_.next( null );
							subscriber_.complete();
						}
					)
				;
			} );
		}
	};
}
