import { Injectable }      from '@angular/core';
import {
	CanActivate,
	Router,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                          from '@angular/http';
import { Observable }      from 'rxjs/Observable';

import {
	Session,
	SessionService
}                          from './session.service';

@Injectable()
export class SessionGuard implements CanActivate {

	public constructor(
		private _router: Router,
		private _http: Http,
		private _sessionService: SessionService
	) {
	};

	public canActivate( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<boolean> {
		var me = this;
		me._sessionService.targetUrl = state_.url;

		// Determine if the session data is still valid and refresh the data if it's not.
		let session: Session = me._sessionService.get();
		if ( session ) {
			let now: number = ( new Date().getTime() / 1000 ) - session.diff;
			let age: number = now - session.created;

			if ( session.valid - now < 10 ) {
				me._router.navigate( [ '/login' ] );
				return Observable.of( false );
			} else if ( age > 60 * 60 ) {
				return me._sessionService.refresh()
					.do( function( success_: boolean ) {
						if ( ! success_ ) {
							me._router.navigate( [ '/login' ] );
						}
					} )
					.catch( function( err_: any ) {
						return Observable.of( false );
					} )
				;
			} else {
				return Observable.of( true );
			}
		} else {
			me._router.navigate( [ '/login' ] );
			return Observable.of( false );
		}

	};

}
