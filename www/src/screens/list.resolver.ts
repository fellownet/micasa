import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	Screen,
	ScreensService
}                          from './screens.service';

@Injectable()
export class ScreensListResolver implements Resolve<Screen[]> {

	public constructor(
		private _router: Router,
		private _screensService: ScreensService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Screen[]> {
		var me = this;
		return this._screensService.getScreens()
			.catch( function( error_: string ) {
				me._router.navigate( [ '/login' ] );
				return Observable.of( null );
			} )
		;
	};
}
