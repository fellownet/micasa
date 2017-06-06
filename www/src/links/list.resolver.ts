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
	LinksService
}                          from './links.service';

@Injectable()
export class LinksListResolver implements Resolve<Link[]> {

	public constructor(
		private _router: Router,
		private _linksService: LinksService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Link[]> {
		var me = this;
		return me._linksService.getLinks( route_.params['device_id'] )
			.catch( function( error_: string ) {
				return Observable.of( [] );
			} )
		;
	};
}
