import { Injectable }              from '@angular/core';
import { Http, Response }          from '@angular/http';
import { Headers, RequestOptions } from '@angular/http';
import { Observable }              from 'rxjs/Observable';
import { Device }                  from './device.service';

export class Script {
	id: number;
	name: string;
	code?: string;
	runs?: number;
	status?: number;
}

@Injectable()
export class ScriptService {

	private _scriptUrlBase = 'api/scripts';

	constructor( private _http: Http ) { };

	getScripts(): Observable<Script[]> {
		return this._http.get( this._scriptUrlBase )
			.map( function( response_: Response ) {
				let body = response_.json();
				return body || false;
			} )
			.catch( this._handleHttpError )
		;
	};

	putScript( script_: Script ): Observable<Script> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		if ( script_.id ) {
			return this._http.put( this._scriptUrlBase + '/' + script_.id, script_, options )
				.map( function( response_: Response ) {
					return response_.json()['script'] || false;
				} )
				.catch( this._handleHttpError )
			;
		} else {
			return this._http.post( this._scriptUrlBase, script_, options )
				.map( function( response_: Response ) {
					return response_.json()['script'] || false;
				} )
				.catch( this._handleHttpError )
			;
		}
	};

	deleteScript( script_: Script ): Observable<boolean> {
		return this._http.delete( this._scriptUrlBase + '/' + script_.id )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
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
