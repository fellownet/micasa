import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	User,
	UsersService,
	ACL
}                          from './users.service';

@Injectable()
export class UserResolver implements Resolve<User> {

	public constructor(
		private _router: Router,
		private _usersService: UsersService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<User> {
		if ( route_.params['user_id'] == 'add' ) {
			return this._usersService.getUserSettings()
				.mergeMap( settings_ => Observable.of( { id: NaN, name: 'New User', username: '', rights: ACL.Viewer, enabled: false, settings: settings_ } ) )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		} else {
			return this._usersService.getUser( +route_.params['user_id'] )
				.catch( () => {
					this._router.navigate( [ '/error' ] );
					return Observable.of( null );
				} )
			;
		}
	};

}
