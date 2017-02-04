import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import { DevicesService }  from './devices.service';

@Injectable()
export class DataResolver implements Resolve<any[]> {

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<any[]> {
		var me = this;
		return this._devicesService.getData( route_.params['device_id'], { group: '5min', range: 1, interval: 'year' } )
			.catch( function( error_: string ) {
				me._router.navigate( [ '/login' ] );
				return Observable.of( null );
			} )
		;
	}
}
