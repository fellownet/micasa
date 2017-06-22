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
import { TimeoutError }    from 'rxjs/util/TimeoutError';

import { User }            from '../users/users.service';

export class Credentials {
	username: string;
	password: string;
	remember: boolean;
};

export class Session {
	user: User;
	token: string;
	valid: number;
	created: number;
	diff: number; // difference in server vs client timestamp
	remember: boolean;
};

export class Error {
	code: number;
	error: string;
	message: string;
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
	private _busy: boolean = false; // recursion check in http
	private _socket?: WebSocket;

	// All requested states are stored locally to prevent requesting the same state from
	// the server multiple times.
	private _states: any = {};

	public session: Observable<Session> = this._session.asObservable();
	public events: Observable<any> = this._events.asObservable();
	public error?: Error;
	public loading: Observable<boolean> = this._loading.asObservable()
		.debounceTime( 200 )
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
		let sessionJson: string = localStorage.getItem( 'session' );
		if ( ! sessionJson ) {
			sessionJson = sessionStorage.getItem( 'session' );
		}
		if ( sessionJson ) {
			try {
				let session: Session = JSON.parse( sessionJson );
				this._session.next( session );
			} catch( e_ ) {
				this._session.next( false );
			}
		} else {
			this._session.next( false );
		}

		// We ourselves are going to listen for a valid session, upon which we're openign a websocket for live updates.
		// A new socket connection needs to be made every time the session token is refreshed.
		this.session
			.filter( session_ => session_ !== null )
			.subscribe( session_ => {
				if ( !! this._socket ) {
					this._socket.close();
					delete this._socket;
				}
				if ( !! session_ ) {
					var loc: Location = window.location;
					var url: string = ( ( loc.protocol === 'https:' ) ? 'wss://' : 'ws://' ) + loc.hostname + ( !! loc.port ? ':' + loc.port : '' ) + '/live/' + session_.token;
					this._socket = new WebSocket( url );
					this._socket.onmessage = event_ => {
						let data: any = JSON.parse( event_.data );
						this._events.next( data );
					};
				}
			} )
		;
	};

	public get(): Session|false|null {
		return this._session.getValue();
	};

	public login( credentials_: Credentials ): Observable<Session> {
		return this.http<Session>( 'post', 'user/login', credentials_ )
			.do( session_ => {
				let now: number = new Date().getTime() / 1000;
				session_.diff = now - session_.created;
				session_.remember = credentials_.remember;

				let storage: Storage = session_.remember ? localStorage : sessionStorage;
				storage.setItem( 'session', JSON.stringify( session_ ) );

				this._session.next( session_ );
			} )
			.catch( error_ => {
				this._session.next( false );
				return Observable.throw( error_ );
			} )
		;
	};

	public refresh(): Observable<Session> {
		return this.http<Session>( 'get', 'user/refresh' )
			.do( session_ => {
				let now: number = new Date().getTime() / 1000;
				session_.diff = now - session_.created;
				session_.remember = ( this.get() as Session ).remember;

				let storage: Storage = session_.remember ? localStorage : sessionStorage;
				storage.setItem( 'session', JSON.stringify( session_ ) );

				this._session.next( session_ );
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
		let session: Session|false|null = this._session.getValue();
		if (
			this._busy == false
			&& !! session
		) {
			let now: number = ( new Date().getTime() / 1000 ) - session.diff;
			let age: number = now - session.created;
			if ( age > 15 * 60 ) {
				this._busy = true;
				return this.refresh()
					.mergeMap( () => {
						return this.http<T>( method_, resource_, params_ );
					} )
					.finally( () => this._busy = false )
				;
			}
		}

		let headersRaw: any = { 'Content-Type': 'application/json' };
		if ( !! session ) {
			headersRaw['Authorization'] = session.token;
		}
		let headers = new Headers( headersRaw );
		let options = new RequestOptions( { headers: headers } );
		let observable = Observable.of( null ).do( () => this._loading.next( this._loading.getValue() + 1 ) );
		if (
			method_ == 'get'
			|| method_ == 'delete'
		) {
			let query: string = '';
			if ( !! params_ ) {
				let parts: any[] = [];
				for( let key in params_ ) {
					parts.push( encodeURIComponent( key ) + '=' + encodeURIComponent( params_[key] ) );
				}
				if (
					resource_.indexOf( '?' ) == -1
					&& parts.length > 0
				) {
					query = '?'
				}
				query += parts.join( '&' );
			}
			observable = observable.mergeMap( () => this._http[method_]( 'api/' + resource_ + query, options ) );
		} else {
			observable = observable.mergeMap( () => this._http[method_]( 'api/' + resource_, params_, options ) );
		}
		return observable
			.timeout( 5000 )
			.map( response_ => {
				let body = response_.json();
				return body.data || null;
			} )
			.catch( response_ => {
				this.error = {
					code: 0,
					error: 'Unknown',
					message: 'An unknown error has occured.'
				};
				if ( response_ instanceof TimeoutError ) {
					this.error.error = 'Connection.Timeout';
					this.error.message = 'The connection timed out.';
				} else if ( response_ instanceof Response ) {
					if ( response_.status == 0 ) {
						this.error.error = 'Connection.Offline';
						this.error.message = 'The connection appears to be offline.';
					} else {
						let body: any = response_.json() || {};
						[ 'code', 'error', 'message' ].forEach( key_ => {
							if ( key_ in body ) {
								this.error[key_] = body[key_];
							}
						} );
					}
				}
				return Observable.throw( this.error );
			} )
			.finally( () => this._loading.next( this._loading.getValue() - 1 ) )
		;
	};

	public store<T>( key_: string, data_: T ): Observable<T> {
		return this.http<any>( 'put', 'user/state/' + key_, { data: data_ } )
			.do( () => this._states[key_] = data_ )
			.map( () => data_ )
		;
	};

	public delete<T>( key_: string ): Observable<T> {
		return this.http<any>( 'delete', 'user/state/' + key_ )
			.mergeMap( () => {
				let data: any = this._states[key_];
				delete this._states[key_];
				return Observable.of( data );
			} )
		;
	};

	public retrieve<T>( key_: string, default_?: T ): Observable<T> {
		// The decision to use our local state cache or fetch from the server should be made once there
		// is a subscriber attached, which doesn't necessarily happens immediately. Therefore we start
		// off with a null observable that immediately returns null once a subscriber is added. Control
		// is then taken over by another observable that returns the requested data.
		return Observable.of( null )
			.mergeMap( () => {
				if ( key_ in this._states ) {
					// While the data is being retrieved, the key holds an observable by itself which
					// other callers can also subscribe to. This prevents data from being fetched more
					// than once if the same key if requested while other requests are pending.
					// NOTE this happens for the screen data that is fetched by the dashboard *and*
					// the menu, both immediately after login.
					if ( this._states[key_] instanceof Observable ) {
						return this._states[key_];
					} else {
						return Observable.of( this._states[key_] );
					}
				} else {
					// Multiple subscribers to one http observable cause multiple request because this
					// observable is executed immediately. Therefore we're creating our own observable
					// and push the data therein once it is received.
					let subject: Subject<T> = new Subject();
					this._states[key_] = subject.asObservable();
					return this.http<T>( 'get', 'user/state/' + key_ )
						.do( data_ => {
							this._states[key_] = data_;
							subject.next( data_ );
							subject.complete();
						 } )
						.catch( error_ => {
							if (
								error_.code == 404 // not found
								&& !! default_
							) {
								return this.store<T>( key_, default_ )
									.do( data_ => {
										subject.next( data_ );
										subject.complete();
									} )
								;
							} else {
								return Observable.throw( error_ );
							}
						} )
						.catch( error_ => {
							// This catch catches both the http for get and put (store). All concurrent
							// subscribers need to have their error handlers called on both occasions.
							subject.error( error_ );
							subject.complete();
							return Observable.throw( error_ );
						} )
					;
				}
			} )
		;
	};

}
