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
		let screenId: any;
		if ( route_.data['dashboard'] ) {
			screenId = 1;
		} else {
			screenId = route_.params['screen_id'];
		}
		if ( screenId == 'new' ) {
			return Observable.of( { id: NaN, name: 'New screen', widgets: [] } );
		} else {
			return this._screensService.getScreen( screenId )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		}
	};
}
