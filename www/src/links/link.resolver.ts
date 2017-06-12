import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import { Setting }         from '../settings/settings.service';
import {
	Link,
	LinksService
}                          from './links.service';

@Injectable()
export class LinkResolver implements Resolve<Link> {

	public constructor(
		private _router: Router,
		private _linksService: LinksService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Link> {
		var me = this;
		if ( route_.params['link_id'] == 'add' ) {
			// If a new link is to be created there's no settings property we can use to build the form, so we need
			// to fetch these settings separately.
			let device_id: number = route_.params['device_id'];
			return me._linksService.getLinkSettings()
				.mergeMap( function( settings_: Setting[] ) {
					return Observable.of( { id: NaN, name: 'New Link', device_id: device_id || NaN, target_device_id: NaN, enabled: false, settings: settings_ } );
				} )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return me._linksService.getLink( +route_.params['link_id'] )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		}
	}

}
