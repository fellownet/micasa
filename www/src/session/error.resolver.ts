import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import {
	Error,
	SessionService
}                          from './session.service';

@Injectable()
export class ErrorResolver implements Resolve<Error> {

	public constructor(
		private _router: Router,
		private _sessionService: SessionService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Error> {
		if ( !! this._sessionService.error ) {
			if ( this._sessionService.error.code == 403 ) {
				// An access denied error results in a logout.
				this._router.navigate( [ '/login' ] );
			} else {
				return Observable.of( this._sessionService.error );
			}
		} else {
			// There's no error; get out of here.
			this._router.navigate( [ '/dashboard' ] );
		}
		return Observable.of( null );
	};

}
