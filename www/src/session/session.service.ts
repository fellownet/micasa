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

	private _session: BehaviorSubject<Session|null> = new BehaviorSubject( null );
	private _events: Subject<any> = new Subject();
	private _storage: Storage;

	// All requested states are stored locally to prevent requesting the same state from
	// the server multiple times.
	private _states: any = {};

	public targetUrl?: string; // the url that was requested before the guard kicked in

	public session: Observable<Session> = this._session.asObservable();
	public events: Observable<any> = this._events.asObservable();

	public constructor(
		private _http: Http
	) {
		var me = this;

		let session: string;
		if ( 'session' in localStorage ) {
			session = localStorage['session'];
			this._storage = localStorage;
		} else if ( 'session' in sessionStorage ) {
			session = sessionStorage['session'];
			this._storage = sessionStorage;
		}
		if ( session ) {
			try {
				this._session.next( JSON.parse( session ) );
			} catch( ex_ ) { }
		}

		/*
		console.log( 'SessionService created' );

		var fTest = function() {
			setTimeout( function() {
				me._events.next( { test: 1 } );
				fTest();
			}, 1000 );
		};
		fTest();
		*/
	};

	public get(): Session|null {
		return this._session.getValue();
	};

	public login( credentials_: Credentials ): Observable<string> {
		var me = this;
		return me.http<Session>( 'post', 'user/login', credentials_ )
			.map( function( session_: Session ) {
				me._processSession( session_, credentials_.remember );
				return me.targetUrl || '/dashboard';
			} )
		;
	};

	public refresh(): Observable<boolean> {
		var me = this;
		return me.http<Session>( 'get', 'user/refresh' )
			.map( function( session_: Session ) {
				me._processSession( session_, me.get().remember );
				return true;
			} )
		;
	};

	public logout(): void {
		this._session.next( null );
		this._states = {};
		delete( localStorage['session'] );
		delete( sessionStorage['session'] );
	};

	public http<T>( action_: string, resource_: string, params_?: any ): Observable<T> {
		let headersRaw: any = { 'Content-Type': 'application/json' };
		let session: Session|null = this.get();
		if ( session ) {
			headersRaw["Authorization"] = session.token;
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

	public store( key_: string, data_: any ): Observable<boolean> {
		this._states[key_] = data_;
		return this.http<any>( 'put', 'user/state/' + key_, { data: data_ } )
			.map( function( result_: any ) {
				return true; // failures do not end up here
			} )
		;
	};

	public delete( key_: string ): Observable<boolean> {
		delete( this._states[key_] );
		return this.http<any>( 'delete', 'user/state/' + key_ )
			.map( function( result_: any ) {
				return true; // failures do not end up here
			} )
		;
	};

	public retrieve<T>( key_: string ): Observable<T> {
		var me = this;
		if ( key_ in me._states ) {
			return Observable.of( me._states[key_] );
		} else {
			return me.http<T>( 'get', 'user/state/' + key_ )
				.do( function( data_: T ) {
					me._states[key_] = data_;
				} )
			;
		}
	};

	private _processSession( session_: Session, remember_: boolean ): void {

		// Store the difference in server- vs client timestamp value.
		let now: number = new Date().getTime() / 1000;
		session_.diff = now - session_.created;

		// Copy over the remember flag from the credentials so whenever login data is
		// used it is known if it's temporary or persistent.
		session_.remember = remember_;

		this._storage = session_.remember ? localStorage : sessionStorage;
		this._storage['session'] = JSON.stringify( session_ );
		this._session.next( session_ );
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
