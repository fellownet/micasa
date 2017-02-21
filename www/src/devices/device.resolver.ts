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
export class DeviceResolver implements Resolve<Device> {

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Device> {
		var me = this;
		return me._devicesService.getDevice( +route_.params['device_id'] )
			.catch( function( error_: string ) {
				me._router.navigate( [ '/login' ] );
				return Observable.of( null );
			} )
		;
	}
}
