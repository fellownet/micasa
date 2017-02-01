import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	Hardware,
	HardwareService
}                          from './hardware.service';

@Injectable()
export class HardwareListResolver implements Resolve<Hardware[]> {

	public constructor(
		private _router: Router,
		private _hardwareService: HardwareService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Hardware[]> {
		var me = this;
		return new Observable( function( subscriber_: any ) {
			me._hardwareService.getList( route_.params['hardware_id'] )
				.subscribe(
					function( hardware_: Hardware[] ) {
						subscriber_.next( hardware_ );
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
}
