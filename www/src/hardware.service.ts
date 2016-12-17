import { Injectable }              from '@angular/core';
import { Http, Response }          from '@angular/http';
import { Headers, RequestOptions } from '@angular/http';
import { Observable }              from 'rxjs/Observable';

@Injectable()
export class Hardware {
	id: number;
	label: string;
	name: string;
	type: string;
}

@Injectable()
export class HardwareService {

	private _hardwareUrlBase = 'api/hardware';

	constructor( private _http: Http ) { };

	getHardware(): Observable<Hardware[]> {
		return this._http.get( this._hardwareUrlBase )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	putHardware( hardware_: Hardware ): Observable<Hardware[]> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id, hardware_, options )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	private _extractData( response_: Response ): Hardware[] {
		let body = response_.json();
		return body || { };
	};

	openzwaveIncludeMode( hardware_: Hardware ): Observable<boolean> {
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id + '/include', {} )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleError )
		;
	};

	exitOpenzwaveIncludeMode( hardware_: Hardware ): Observable<boolean> {
		return this._http.delete( this._hardwareUrlBase + '/' + hardware_.id + '/include', {} )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleError )
		;
	};

	openzwaveExcludeMode( hardware_: Hardware ): Observable<boolean> {
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id + '/exclude', {} )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleError )
		;
	};

	exitOpenzwaveExcludeMode( hardware_: Hardware ): Observable<boolean> {
		return this._http.delete( this._hardwareUrlBase + '/' + hardware_.id + '/exclude', {} )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleError )
		;
	};

	private _handleError( response_: Response | any ) {
		let message: string;
		if ( response_ instanceof Response ) {
			message = 'an unspecified error to be specified later on';
		}
		return Observable.throw( message );
	};

}
