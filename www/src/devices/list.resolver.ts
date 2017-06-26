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
		} else if ( 'plugin_id' in route_.params ) {
			return this._devicesService.getDevices( route_.params['plugin_id'] )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else if ( 'script_id' in route_.params ) {
			return this._devicesService.getDevicesForScript( route_.params['script_id'] )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return this._devicesService.getDevices()
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		}
	};

}
