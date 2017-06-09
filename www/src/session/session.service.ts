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
	private _loading: BehaviorSubject<number> = new BehaviorSubject( 0 );
	private _forcedAuthToken: string;
	private _busy: boolean = false; // recursion check in http
	private _socket?: WebSocket;

	// All requested states are stored locally to prevent requesting the same state from
	// the server multiple times.
	private _states: any = {};

	public session: Observable<Session> = this._session.asObservable();
	public events: Observable<any> = this._events.asObservable();
	public loading: Observable<boolean> = this._loading.asObservable()
		.debounceTime( 250 )
		.merge(
			// The value 1 is allowed to pass regardless of the debounce timer which ensures that loading indicators
			// pop up immediately once the first http request starts.
			// NOTE the delay makes sure that the value is pushed *after* angular as done it's change detection and
			// prevents the notorious ExpressionChangedAfterItHasBeenCheckedError from being throwed.
			this._loading.asObservable().filter( loading_ => loading_ == 1  ).delay( 1 )
		)
		.map( loading_ => loading_ > 0 )
	;

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

		// We ourselves are going to listen for a valid session, upon which we're openign a websocket for live updates.
		// A new socket connection needs to be made every time the session token is refreshed.
		me.session
			.filter( session_ => session_ !== null )
			.subscribe( function( session_: Session|false ) {
				if ( !! me._socket ) {
					me._socket.close();
					delete me._socket;
				}
				if ( !! session_ ) {
					var loc: Location = window.location;
					var url: string = ( ( loc.protocol === 'https:' ) ? 'wss://' : 'ws://' ) + loc.hostname + ( !! loc.port ? ':' + loc.port : '' ) + '/live/' + session_.token;
					me._socket = new WebSocket( url );
					me._socket.onmessage = function( event_: any ) {
						let data: any = JSON.parse( event_.data );
						me._events.next( data );
					};
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

	public http<T>( method_: string, resource_: string, params_?: any ): Observable<T> {
		var me = this;
		let session: Session|false|null = me._session.getValue();

		if (
			me._busy == false
			&& !! session
		) {
			let now: number = ( new Date().getTime() / 1000 ) - session.diff;
			let age: number = now - session.created;
			if ( age > 15 * 60 ) {
				me._busy = true;
				return this.refresh()
					.mergeMap( function() {
						return me.http<T>( method_, resource_, params_ )
							.do( function() {
								me._busy = false;
							} )
						;
					} )
				;
			}
		}

		let headersRaw: any = { 'Content-Type': 'application/json' };
		if ( me._forcedAuthToken ) {
			headersRaw["Authorization"] = me._forcedAuthToken;
		} else {
			if ( !! session ) {
				headersRaw["Authorization"] = session.token;
			}
		}
		let headers = new Headers( headersRaw );
		let options = new RequestOptions( { headers: headers } );
		let before = Observable.of( null ).do( () => me._loading.next( me._loading.getValue() + 1 ) );
		if (
			method_ == 'get'
			|| method_ == 'delete'
		) {
			return before
				.mergeMap( function() {
					return me._http[method_]( 'api/' + resource_, options )
						.map( me._extractData )
						.catch( me._handleHttpError )
						.finally( () => me._loading.next( me._loading.getValue() - 1 ) )
					;
				} )
			;
		} else {
			return before
				.mergeMap( function() {
					return me._http[method_]( 'api/' + resource_, params_, options )
						.map( me._extractData )
						.catch( me._handleHttpError )
						.finally( () => me._loading.next( me._loading.getValue() - 1 ) )
					;
				} )
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
