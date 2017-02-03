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
			return this._devicesService.getDevices( route_.params['hardware_id'], route_.params['script_id'] )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		}
	};
}
