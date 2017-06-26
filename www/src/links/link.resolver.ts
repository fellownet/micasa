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
		if ( route_.params['link_id'] == 'add' ) {
			let device_id: number = route_.params['device_id'];
			return this._linksService.getLinkSettings( device_id )
				.mergeMap( settings_ => Observable.of( { id: NaN, name: 'New Link', device_id: device_id || NaN, target_device_id: NaN, enabled: false, settings: settings_ } ) )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return this._linksService.getLink( +route_.params['link_id'], route_.params['device_id'] )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		}
	};

}
