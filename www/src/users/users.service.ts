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
	remember: boolean;
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
	enabled: boolean;
}

export class Login {
	user: User;
	token: string;
	valid: number;
	created: number;
	diff: number; // difference in server vs client timestamp
	remember: boolean;
	settings?: any;
}

@Injectable()
export class UsersService implements CanActivate {

	private _userUrlBase = 'api/users';
	private _authUrlBase = 'api/user';

	private _login?: Login;

	// The redirectUrl property holds the url that's being navigated to while checking for
	// logged in status.
	redirectUrl: string;

	constructor(
		private _router: Router,
		private _http: Http
	) {
		try {
			this._login = JSON.parse( localStorage.getItem( 'login' ) );
		} catch( ex_ ) {
			this._login = null;
		}
	};

	// This method is called for all routes that require an active user. Pretty much all routes
	// require an active user, except for the login route.
	canActivate( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): boolean {
		var me = this;
		me.redirectUrl = state_.url;

		// Invalidate logged in user if the valid time has passed or retrieve a new token
		// if the login is of certain age.
		if ( me._login ) {

			let now: number = ( new Date().getTime() / 1000 ) - me._login.diff;
			let age: number = now - me._login.created;

			if ( me._login.valid - now < 10 ) {
				me._router.navigate( [ '/login' ] );
				return false;
			} else if ( age > 60 * 60 ) { // one hour
				me._refreshLogin()
					.subscribe(
						function( login_: Login ) {

							// Update the retrieved login object with properties from the existing
							// login object.
							login_.settings = me._login.settings;
							login_.remember = me._login.remember;

							// Update the difference in server- vs client timestamp value.
							let now: number = new Date().getTime() / 1000;
							login_.diff = now - login_.created;

							// Then update our login object with the new one.
							me._login = login_;

							if ( me._login.remember ) {
								localStorage.setItem( 'login', JSON.stringify( me._login ) );
							}

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
			return new Observable( function( subscriber_: any ) {
				me.getUser( +route_.params['user_id'] )
					.subscribe(
						function( user_: User ) {
							subscriber_.next( user_ );
							subscriber_.complete();
						},
						function( error_: string ) {
							me._router.navigate( [ '/users' ] );
							subscriber_.next( null );
							subscriber_.complete();
						}
					)
				;
			} );
		}
	}

	getUsers(): Observable<User[]> {
		let headers = new Headers( { 'Authorization': this._login.token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._userUrlBase, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getUser( id_: Number ): Observable<User> {
		let headers = new Headers( { 'Authorization': this._login.token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._userUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	putUser( user_: User ): Observable<User> {
		let headers = new Headers( {
			'Content-Type' : 'application/json',
			'Authorization': this._login.token
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
		let headers = new Headers( { 'Authorization': this._login.token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._userUrlBase + '/' + user_.id, options )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	// Authorization methods

	private _doLogin( credentials_: Credentials ): Observable<Login> {
		var me = this;
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		return me._http.post( me._authUrlBase + '/login', credentials_, options )
			.map( me._extractData )
			.catch( me._handleHttpError )
		;
	};

	private _refreshLogin(): Observable<Login> {
		var me = this;
		let headers = new Headers( { 'Authorization': me._login.token } );
		let options = new RequestOptions( { headers: headers } );
		return me._http.get( me._authUrlBase + '/refresh', options )
			.map( me._extractData )
			.catch( me._handleHttpError )
		;
	};

	private _getSettings( token_: string ): Observable<any> {
		var me = this;
		let headers = new Headers( { 'Authorization': token_ } );
		let options = new RequestOptions( { headers: headers } );
		return me._http.get( me._authUrlBase + '/settings', options )
			.map( me._extractData )
			.catch( me._handleHttpError )
		;
	};

	doLogin( credentials_: Credentials ): Observable<boolean> {
		var me = this;
		return new Observable<boolean>( function( subscriber_: any ) {
			me._doLogin( credentials_ )
				.subscribe(
					function( login_: Login ) {

						// Store the difference in server- vs client timestamp value.
						let now: number = new Date().getTime() / 1000;
						login_.diff = now - login_.created;

						// Copy over the remember flag from the credentials so whenever login data is
						// used it is known if it's temporary of persistent.
						login_.remember = credentials_.remember;

						// The settings for the current are fetched. Only after these settings are
						// successfully retrieved the login is accepted.
						me._getSettings( login_.token )
							.subscribe(
								function( settings_: any ) {

									// Prepare the screens array in settings. It should contain at least
									// the dashboard.
									if ( ! ( 'screens' in settings_ ) ) {
										settings_.screens = [ {
											index  : 0,
											name   : 'Dashboard',
											widgets: []
										} ];
									}

									// Add the settings object to the previously received login object.
									login_.settings = settings_;

									me._login = login_;
									if ( me._login.remember ) {
										localStorage.setItem( 'login', JSON.stringify( me._login ) );
									} else {
										localStorage.removeItem( 'login' );
									}

									subscriber_.next( true );
									subscriber_.complete();
								},
								function( error_: string ) {
									subscriber_.error( error_ );
									subscriber_.complete();
								}
							)
						;
					},
					function( error_: string ) {
						subscriber_.error( error_ );
						subscriber_.complete();
					}
				)
			;
		} );
	};

	doLogout(): void {
		this._login = null;
		localStorage.removeItem( 'login' );
	};

	isLoggedIn( acl_?: ACL ): boolean {
		if ( acl_ ) {
			return this._login && this._login.user.rights >= acl_;
		} else {
			return !!this._login;
		}
	};

	getLogin(): Login {
		return this._login;
	};

	syncSettings( key_?: string ): Observable<any> {
		let headers = new Headers( {
			'Content-Type' : 'application/json',
			'Authorization': this._login.token
		} );
		let options = new RequestOptions( { headers: headers } );

		if ( this._login.remember ) {
			localStorage.setItem( 'login', JSON.stringify( this._login ) );
		}

		if ( key_ ) {
			let setting: any = {};
			setting[key_] = ( key_ in this._login.settings ? this._login.settings[key_] : {} );
			return this._http.put( this._authUrlBase + '/settings', { settings: setting }, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		} else {
			return this._http.put( this._authUrlBase + '/settings', { settings: this._login.settings }, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		}
	};

	// Http request handlers.

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
