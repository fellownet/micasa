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
export class TimerResolver implements Resolve<Timer> {

	public constructor(
		private _router: Router,
		private _timersService: TimersService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Timer> {
		if ( route_.params['timer_id'] == 'add' ) {
			let device_id: number = route_.params['device_id'];
			return this._timersService.getTimerSettings( device_id )
				.mergeMap( settings_ => Observable.of( { id: NaN, name: 'New Timer', cron: '* * * * *', device_id: device_id || NaN, enabled: false, scripts: [], settings: settings_ } ) )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return this._timersService.getTimer( +route_.params['timer_id'], route_.params['device_id'] )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		}
	};

}
