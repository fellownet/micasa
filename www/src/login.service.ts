import { Injectable }  from '@angular/core';
import {
	CanActivate,
	Router,
	ActivatedRouteSnapshot,
	RouterStateSnapshot
}                      from '@angular/router';
import { Observable }  from 'rxjs/Observable';
import { User, ACL }   from './user.service';

import 'rxjs/add/observable/of';
import 'rxjs/add/operator/do';
import 'rxjs/add/operator/delay';

export class Session {
	user: User;
	created: Date;
	expire: Date;
}

@Injectable()
export class LoginService implements CanActivate {

	private _session: Session;
	redirectUrl: string;

	constructor( private _router: Router ) {
	}

	canActivate( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): boolean {
		this.redirectUrl = state_.url;
		if ( this.isLoggedIn() ) {
			return true;
		} else {
			this._router.navigate( [ '/login' ] );
			return false;
		}
	}

	login( username_: string, password_: string ): Observable<boolean> {
		// store a token after login with it's own expiration- and creation date
		// this token should be valid for a week or so
		// if the token is one hour old, request a new token with the old one
		// this way the user will only need to login after a week of non-use

		return Observable.of( true ).delay( 250 /* msec */ ).do( val => this._session = {
			user: { username: username_, fullname: 'Bob Kersten (' + username_ + ')', acl: ACL.Admin | ACL.User | ACL.Viewer  },
			created: new Date(),
			expire: new Date()
		} );
	}

	logout(): void {
		this._session = null;
	}

	isLoggedIn( acl_?: ACL ): boolean {

		return !!this._session && (
			!acl_ // no acl provided means just check for logged in status
			|| ( this._session.user.acl & acl_ ) == acl_
		);
	}

}
