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
			if ( 'device_id' in route_.params ) {
				return Observable.of( { id: NaN, name: 'New timer', cron: '* * * * *', enabled: false, device_id: route_.params['device_id'] } );
			} else {
				return Observable.of( { id: NaN, name: 'New timer', cron: '* * * * *', enabled: false, scripts: [] } );
			}
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
