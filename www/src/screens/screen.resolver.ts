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
	ScreensService,
}                          from './screens.service';

@Injectable()
export class ScreenResolver implements Resolve<Screen> {

	public constructor(
		private _router: Router,
		private _screensService: ScreensService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Screen> {
		var me = this;
		if ( route_.data['dashboard'] ) {
			return this._screensService.getScreen( 1 )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		}
		if ( ! ( 'screen_id' in route_.params ) ) {
			return Observable.of( { id: NaN, name: 'New screen', widgets: [] } );
		} else {
			return this._screensService.getScreen( +route_.params['screen_id'] )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/dashboard' ] );
					return Observable.of( null );
				} )
			;
		}
	};
}
