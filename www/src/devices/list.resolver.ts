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
		if ( route_.params['script_id'] == 'add' ) {
			return Observable.of( [] );
		} else if ( 'hardware_id' in route_.params ) {
			// The hardware edit screen also shows disabled devices.
			return this._devicesService.getDevices( { hardware_id: route_.params['hardware_id'] } )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else if ( 'script_id' in route_.params ) {
			return this._devicesService.getDevices( { script_id: route_.params['script_id'], enabled: 1 } )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return this._devicesService.getDevices( { enabled: 1 } )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		}
	};

}
