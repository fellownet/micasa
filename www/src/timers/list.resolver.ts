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
		return new Observable( function( subscriber_: any ) {
			me._timersService.getTimers( route_.params['device_id'] )
				.subscribe(
					function( timers_: Timer[] ) {
						subscriber_.next( timers_ );
						subscriber_.complete();
					},
					function( error_: string ) {
						me._router.navigate( [ '/login' ] );
						subscriber_.next( null );
						subscriber_.complete();
					}
				)
			;
		} );
	};
}
