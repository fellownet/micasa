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
		return new Observable( function( subscriber_: any ) {
			me._devicesService.getDevice( +route_.params['device_id'] )
				.subscribe(
					function( device_: Device ) {
						subscriber_.next( device_ );
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
