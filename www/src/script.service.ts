import { Injectable }              from '@angular/core';
import { Http, Response }          from '@angular/http';
import { Headers, RequestOptions } from '@angular/http';
import { Observable }              from 'rxjs/Observable';
import { Device }                  from './device.service';

@Injectable()
export class Script {
	id: number;
	name: string;
	code: string;
	runs: number;
	status: number;
}

@Injectable()
export class ScriptService {

	private _scriptUrlBase = 'api/scripts';

	constructor( private _http: Http ) { };

	getScripts(): Observable<Script[]> {
		return this._http.get( this._scriptUrlBase )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	putScript( script_: Script ): Observable<boolean> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		if ( script_.id ) {
			return this._http.put( this._scriptUrlBase + '/' + script_.id, script_, options )
				.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
				} )
				.catch( this._handleError )
			;
		} else {
			return this._http.post( this._scriptUrlBase, script_, options )
				.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
				} )
				.catch( this._handleError )
			;
		}
	};

	deleteScript( script_: Script ): Observable<boolean> {
		return this._http.delete( this._scriptUrlBase + '/' + script_.id )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleError )
		;
	};

	private _extractData( response_: Response ): Script[] {
		let body = response_.json();
		return body || { };
	};

	private _handleError( response_: Response | any ) {
		let message: string;
		if ( response_ instanceof Response ) {
			message = 'an unspecified error to be specified later on';
		}
		return Observable.throw( message );
	};

}
