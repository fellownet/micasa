import { Injectable }      from '@angular/core';
import {
	CanActivate,
	Router,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import { SessionService }  from './session.service';

@Injectable()
export class SessionGuard implements CanActivate {

	public constructor(
		private _router: Router,
		private _sessionService: SessionService
	) {
	};

	public canActivate( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<boolean> {
		// The observable needs to block until the session service is fully initialized. Therefore we filter
		// out all null values from the observable until there's either a false or a full session object pushed
		// into the stream. This way, the ui is not rendered until the session service is fully initialized.
		return this._sessionService.session
			.filter( session_ => session_ !== null )
			.map( session_ => !!session_ )
			.do( allow_ => {
				if ( ! allow_ ) {
					this._router.navigate( [ '/login' ] );
				}
			} )
		;
	};

}
