import { Injectable }      from '@angular/core';
import {
	CanActivate,
	Router,
	Resolve,
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

export class Credentials {
	username: string;
	password: string;
}

export enum ACL {
	Public = 1,
	Viewer = 2,
	User = 3,
	Installer = 4,
	Admin = 99
}

export class User {
	id: number;
	name: string;
	username: string;
	password?: string;
	rights: ACL;
}

@Injectable()
export class UsersService implements CanActivate {

	private _userUrlBase = 'api/users';

	private _loggedInUser?: any; // not an exact user
	private _loggedInToken?: string;

	// The redirectUrl property holds the url that's being navigated to while checking for
	// logged in status.
	redirectUrl: string;

	constructor(
		private _router: Router,
		private _http: Http
	) {
		try {
			this._loggedInUser = JSON.parse( localStorage.getItem( 'loggedInUser' ) );
			this._loggedInToken = localStorage.getItem( 'loggedInToken' );
		} catch( ex_ ) {
			this._loggedInUser = this._loggedInToken = null;
		}
	};

	// This method is called for all routes that require an active user. Pretty much all routes
	// require an active user, except for the login route.
	canActivate( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): boolean {
		var me = this;
		me.redirectUrl = state_.url;

		// Invalidate logged in user if the valid time has passed.
		if ( me._loggedInUser ) {
			let now: number = new Date().getTime() / 1000;
			let diff: number = me._loggedInUser.valid - now;
			if ( diff < 10 ) {
				me._router.navigate( [ '/login' ] );
				return false;
			} else if ( diff < 60 * 5 ) {
				me.refreshLogin()
					.subscribe( // without a subscriber observer is never executed
						function( success_: boolean ) {
							me._router.navigate( [ me.redirectUrl ] );
						},
						function( error_: string ) {
							me._router.navigate( [ '/login' ] );
						}
					)
				;
				return false;
			} else {
				return true;
			}
		} else {
			me._router.navigate( [ '/login' ] );
			return false;
		}
	}

	// The resolve method gets executed by the router before a route is being navigated to. This
	// method fetches the user and injects it into the router state. If this fails the router
	// is instructed to navigate away from the route before the observer is complete.
	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<User> {
		var me = this;
		if ( route_.params['user_id'] == 'add' ) {
			return Observable.of( { id: NaN, name: 'New user', username: '', rights: ACL.Viewer, enabled: false } );
		} else {
			return new Observable( function( observer_: any ) {
				me.getUser( +route_.params['user_id'] )
					.subscribe(
						function( user_: User ) {
							observer_.next( user_ );
							observer_.complete();
						},
						function( error_: string ) {
							me._router.navigate( [ '/users' ] );
							observer_.next( null );
							observer_.complete();
						}
					)
				;
			} );
		}
	}

	getUsers(): Observable<User[]> {
		let headers = new Headers( { 'Authorization': this.getLoggedInToken() } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._userUrlBase, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getUser( id_: Number ): Observable<User> {
		let headers = new Headers( { 'Authorization': this.getLoggedInToken() } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._userUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	putUser( user_: User ): Observable<User> {
		let headers = new Headers( {
			'Content-Type' : 'application/json',
			'Authorization': this.getLoggedInToken()
		} );
		let options = new RequestOptions( { headers: headers } );
		if ( user_.id ) {
			return this._http.put( this._userUrlBase + '/' + user_.id, user_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		} else {
			return this._http.post( this._userUrlBase, user_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		}
	};

	deleteUser( user_: User ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this.getLoggedInToken() } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._userUrlBase + '/' + user_.id, options )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	doLogin( credentials_: Credentials ): Observable<boolean> {
		var me = this;
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		return me._http.post( me._userUrlBase + '/login', credentials_, options )
			.map( function( response_: Response ) {
				let body = response_.json();
				me._processLogin( body.data );
				return true;
			} )
			.catch( me._handleHttpError )
		;
	};

	doLogout(): void {
		this._loggedInUser = this._loggedInToken = null;
		localStorage.removeItem( 'loggedInUser' );
		localStorage.removeItem( 'loggedInToken' );
	};

	refreshLogin(): Observable<boolean> {
		var me = this;
		let headers = new Headers( { 'Authorization': me.getLoggedInToken() } );
		let options = new RequestOptions( { headers: headers } );
		return me._http.get( me._userUrlBase + '/login', options )
			.map( function( response_: Response ) {
				let body = response_.json();
				me._processLogin( body.data );
				return true;
			} )
			.catch( me._handleHttpError )
		;
	};

	isLoggedIn( acl_?: ACL ): boolean {
		if ( acl_ ) {
			return this._loggedInUser && this._loggedInUser.rights >= acl_;
		} else {
			return !!this._loggedInUser;
		}
	};

	getLoggedInToken(): string {
		return this._loggedInToken;
	};

	private _processLogin( login_: any ) {
		if ( login_.user ) {
			this._loggedInUser = login_.user;
			localStorage.setItem( 'loggedInUser', JSON.stringify( this._loggedInUser ) );
		}
		if ( login_.token ) {
			this._loggedInToken = login_.token;
			localStorage.setItem( 'loggedInToken', this._loggedInToken );
		}
	};

	private _extractData( response_: Response ) {
		let body = response_.json();
		return body.data || null;
	};

	private _handleHttpError( response_: Response | any ) {
		let message: string;
		if ( response_ instanceof Response ) {
			const body = response_.json() || '';
			const error = body.message || JSON.stringify( body );
			message = `${response_.status} - ${response_.statusText || ''} ${error}`;
		} else {
			message = response_.message ? response_.message : response_.toString();
		}
		return Observable.throw( message );
	};

}
