import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	Link,
	DevicesService
}                          from '../devices.service';

@Injectable()
export class LinksListResolver implements Resolve<Link[]> {

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Link[]> {
		var me = this;
		return me._devicesService.getLinks( route_.params['device_id'] )
			.catch( function( error_: string ) {
				// TODO somehow do not resolve when device is not of type switch
				//me._router.navigate( [ '/login' ] );
				return Observable.of( [] );
			} )
		;
	};
}
