// http://blog.angular-university.io/how-to-build-angular2-apps-using-rxjs-observable-data-services-pitfalls-to-avoid/

import { Injectable }      from '@angular/core';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                          from '@angular/http';
import { Subject }         from 'rxjs/Subject';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';
import { Observable }      from 'rxjs/Observable';
import { Subscriber }      from 'rxjs/Subscriber';

import { User }            from '../users/users.service';

export class Credentials {
	username: string;
	password: string;
	remember: boolean;
}

export class Session {
	user: User;
	token: string;
	valid: number;
	created: number;
	diff: number; // difference in server vs client timestamp
	remember: boolean;
}

@Injectable()
export class SessionService {

	// Components that are dependant on the state of the session need to subscribe to the public session
	// observable which emits null if still initializing, false when there's no valid session, or a full
	// session instance when there is one. This way, once a proper session is pushed into the observable
	// stream the session is preloaded and completely initialized.

	private _session: BehaviorSubject<Session|false|null> = new BehaviorSubject( null );
	private _events: Subject<any> = new Subject();
	private _forcedAuthToken: string;

	// All requested states are stored locally to prevent requesting the same state from
	// the server multiple times.
	private _states: any = {};

	public session: Observable<Session> = this._session.asObservable();
	public events: Observable<any> = this._events.asObservable();

	public constructor(
		private _http: Http
	) {
		var me = this;

		let sessionJson: string = localStorage.getItem( 'session' );
		if ( ! sessionJson ) {
			sessionJson = sessionStorage.getItem( 'session' );
		}
		if ( sessionJson ) {
			try {
				let session: Session = JSON.parse( sessionJson );

				me._forcedAuthToken = session.token;
				me._preload().subscribe(
					function( success_: boolean ) {
						me._session.next( session );
						delete( me._forcedAuthToken );
					},
					function( error_: string ) {
						me._session.next( false );
					}
				);
			} catch( e_ ) {
				me._session.next( false );
			}
		} else {
			me._session.next( false );
		}

		// Periodically check for session validity or expiration and refresh the token of it's older than
		// a predefined age. We're not bothering with unsubscribing from this observable because it should
		// exist during the entire lifespan of the app.
		Observable.interval( 1000 * 60 )
			.filter( () => !!me._session.getValue() )
			.map( () => me._session.getValue() as Session )
			.subscribe( function( session_: Session ) {
				let now: number = ( new Date().getTime() / 1000 ) - session_.diff;
				let age: number = now - session_.created;
				if ( session_.valid - now < 60 ) {
					me._session.next( false );
				} else if ( age > 60 * 60 ) {
					me.refresh().subscribe();
				}
			} )
		;
	};

	public get(): Session|false|null {
		return this._session.getValue();
	};

	public login( credentials_: Credentials ): Observable<boolean> {
		var me = this;
		return me.http<Session>( 'post', 'user/login', credentials_ )
			.mergeMap( function( session_: Session ) {

				let now: number = new Date().getTime() / 1000;
				session_.diff = now - session_.created;
				session_.remember = credentials_.remember;

				let storage: Storage = session_.remember ? localStorage : sessionStorage;
				storage.setItem( 'session', JSON.stringify( session_ ) );

				me._forcedAuthToken = session_.token;
				return me._preload()
					.map( function( success_: boolean ) {
						me._session.next( session_ );
						delete( me._forcedAuthToken );
						return true;
					} )
				;
			} )
			.catch( function( error_: string ) {
				me._session.next( false );
				return Observable.throw( error_ );
			} )
		;
	};

	public refresh(): Observable<boolean> {
		var me = this;
		return me.http<Session>( 'get', 'user/refresh' )
			.map( function( session_: Session ) {

				let now: number = new Date().getTime() / 1000;
				session_.diff = now - session_.created;
				session_.remember = ( me.get() as Session ).remember;

				let storage: Storage = session_.remember ? localStorage : sessionStorage;
				storage.setItem( 'session', JSON.stringify( session_ ) );

				me._session.next( session_ );
				return true;
			} )
			.catch( function( error_: string ) {
				me._session.next( false );
				return Observable.throw( error_ );
			} )
		;
	};

	public logout(): void {
		this._session.next( false );
		this._states = {};
		localStorage.clear();
		sessionStorage.clear();
	};

	public http<T>( action_: string, resource_: string, params_?: any ): Observable<T> {
		let headersRaw: any = { 'Content-Type': 'application/json' };
		if ( this._forcedAuthToken ) {
			headersRaw["Authorization"] = this._forcedAuthToken;
		} else {
			let session: Session|false|null = this._session.getValue();
			if ( session ) {
				headersRaw["Authorization"] = session.token;
			}
		}
		let headers = new Headers( headersRaw );
		let options = new RequestOptions( { headers: headers } );
		if (
			action_ == 'get'
			|| action_ == 'delete'
		) {
			return this._http[action_]( 'api/' + resource_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		} else {
			return this._http[action_]( 'api/' + resource_, params_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		}
	};

	public store<T>( key_: string, data_: T ): Observable<T> {
		this._states[key_] = data_;
		return this.http<any>( 'put', 'user/state/' + key_, { data: data_ } )
			.map( function( result_: any ) {
				return data_;
			} )
		;
	};

	public delete( key_: string ): Observable<boolean> {
		delete( this._states[key_] );
		return this.http<any>( 'delete', 'user/state/' + key_ )
			.map( function( result_: any ) {
				return true;
			} )
		;
	};

	public retrieve<T>( key_: string, default_?: T ): Observable<T> {
		var me = this;

		// The decision to use our local state cache or fetch from the server should be made once there
		// is a subscriber attached, which doesn't necessarily happens immediately.
		// NOTE for instance when using an async pipe in templates that are conditionally rendered, for
		// instance the menu uses this technique when rendering the screen items.

		return Observable.of( null )
			.mergeMap( function() {
				if ( key_ in me._states ) {
					return Observable.of( me._states[key_] );
				} else {
					return me.http<T>( 'get', 'user/state/' + key_ )
						.do( function( data_: T ) {
							me._states[key_] = data_;
						} )
						.catch( function( error_: string ) {
							if ( default_ ) {
								return me.store<T>( key_, default_ );
							} else {
								return Observable.throw( error_ );
							}
						} )
					;
				}
			} )
		;
	};

	private _preload(): Observable<boolean> {
		// NOTE the preload method receies a session object because it needs to make authenticated calls
		// without the session being persistent yet.
		return Observable.merge(
			this.retrieve<any>( 'screens', [ { id: 1, name: 'Dashboard', widgets: [] } ] ),
			this.retrieve<any>( 'uistate', { } )
		).every( success_ => !!success_ );
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
