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
import { Setting }         from '../../settings/settings.service';

@Injectable()
export class LinkResolver implements Resolve<Link> {

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Link> {
		var me = this;
		if ( route_.params['link_id'] == 'add' ) {
			// If a new link is to be created there's no settings property we can use to build the form, so we need
			// to fetch these settings separately.
			return me._devicesService.getLinkSettings( +route_.params['device_id'] )
				.mergeMap( function( settings_: Setting[] ) {
					return Observable.of( { id: NaN, name: 'New Link', device_id: NaN, target_device_id: NaN, enabled: false, settings: settings_ } );
				} )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return me._devicesService.getLink( +route_.params['device_id'], +route_.params['link_id'] )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		}
	}
}
