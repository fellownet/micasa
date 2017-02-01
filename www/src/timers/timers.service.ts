import { Injectable }   from '@angular/core';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                       from '@angular/http';
import { Observable }   from 'rxjs/Observable';
import { UsersService } from '../users/users.service';

export class Timer {
	id: number;
	name: string;
	cron: string;
	enabled: boolean;
	scripts?: number[];
	device_id?: number;
	value?: string;
}

@Injectable()
export class TimersService {

	private _timerUrlBase = 'api/timers';

	public constructor(
		private _http: Http,
		private _usersService: UsersService
	) {
	};

	public getTimers( deviceId_?: number ): Observable<Timer[]> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		let uri: string = this._timerUrlBase;
		if ( deviceId_ ) {
			uri += '?device_id=' + deviceId_;
		}
		return this._http.get( uri, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public getTimer( id_: number, deviceId_?: number ): Observable<Timer> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		let uri: string = this._timerUrlBase + '/' + id_;
		if ( deviceId_ ) {
			uri += '?device_id=' + deviceId_;
		}
		return this._http.get( uri, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public putTimer( timer_: Timer ): Observable<Timer> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		if ( timer_.id ) {
			return this._http.put( this._timerUrlBase + '/' + timer_.id, timer_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		} else {
			return this._http.post( this._timerUrlBase, timer_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		}
	};

	public deleteTimer( timer_: Timer ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._timerUrlBase + '/' + timer_.id, options )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
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
