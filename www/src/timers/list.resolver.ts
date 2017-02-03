import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	Timer,
	TimersService
}                          from './timers.service';

@Injectable()
export class TimersListResolver implements Resolve<Timer[]> {

	public constructor(
		private _router: Router,
		private _timersService: TimersService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Timer[]> {
		var me = this;
		return this._timersService.getTimers( route_.params['device_id'] )
			.catch( function( error_: string ) {
				me._router.navigate( [ '/login' ] );
				return Observable.of( null );
			} )
		;
	};
}
