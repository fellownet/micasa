import { Injectable }   from '@angular/core';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                       from '@angular/http';
import { Observable }   from 'rxjs/Observable';
import { UsersService } from '../users/users.service';

export class Script {
	id: number;
	name: string;
	code?: string;
	device_ids?: number[];
	enabled: boolean;
}

@Injectable()
export class ScriptsService {

	private _scriptUrlBase = 'api/scripts';

	public constructor(
		private _http: Http,
		private _usersService: UsersService
	) {
	};

	public getScripts(): Observable<Script[]> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._scriptUrlBase, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public getScript( id_: Number ): Observable<Script> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._scriptUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public putScript( script_: Script ): Observable<Script> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		if ( script_.id ) {
			return this._http.put( this._scriptUrlBase + '/' + script_.id, script_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		} else {
			return this._http.post( this._scriptUrlBase, script_, options )
				.map( this._extractData )
				.catch( this._handleHttpError )
			;
		}
	};

	public deleteScript( script_: Script ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._scriptUrlBase + '/' + script_.id, options )
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
