import { Injectable }      from '@angular/core';
import {
	CanActivate,
	Router,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import { SessionService }  from './session.service';
import { ACL }             from '../users/users.service';

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
		let acl: ACL = route_.data.acl || ACL.Viewer;
		return this._sessionService.session
			.filter( session_ => session_ !== null )
			.map( session_ => {
				return (
					!! session_
					&& session_.user.rights >= acl
				);

			 } )
			.do( allow_ => {
				if ( ! allow_ ) {
					if ( !! route_.data.alt ) {
						let route: string[] = Array.isArray( route_.data.alt ) ? route_.data.alt : route_.data.alt.split( '/' );
						route.forEach( ( segment_, index_ ) => {
							if (
								segment_.substr( 0, 1 ) == ':'
								&& segment_.substr( 1 ) in route_.params
							) {
								route[index_] = route_.params[segment_.substr( 1 )];
							}
						} );
						this._router.navigate( route );
					} else {
						this._router.navigate( [ '/login' ] );
					}
				}
			} )
		;
	};

}
