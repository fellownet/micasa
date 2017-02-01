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
		return new Observable( function( subscriber_: any ) {
			me._devicesService.getData( +route_.params['device_id'] )
				.subscribe(
					function( data_: any[] ) {
						subscriber_.next( data_ );
						subscriber_.complete();
					},
					function( error_: string ) {
						me._router.navigate( [ '/devices' ] );
						subscriber_.next( null );
						subscriber_.complete();
					}
				)
			;
		} );
	}
}
