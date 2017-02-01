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
	UsersService
}                          from './users.service';

@Injectable()
export class UsersListResolver implements Resolve<User[]> {

	public constructor(
		private _router: Router,
		private _usersService: UsersService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<User[]> {
		var me = this;
		return new Observable( function( subscriber_: any ) {
			me._usersService.getUsers()
				.subscribe(
					function( users_: User[] ) {
						subscriber_.next( users_ );
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
