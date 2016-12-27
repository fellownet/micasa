import { Injectable }      from '@angular/core';
import {
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

export class Hardware {
	id: number;
	label: string;
	name: string;
	type: string;
	state: string;
	parent?: Hardware;
}

@Injectable()
export class HardwareService {

	private _hardwareUrlBase = 'api/hardware';

	constructor(
		private _router: Router,
		private _http: Http
	) {
	};

	// The resolve method gets executed by the router before a route is being navigated to. This
	// method fetches the hardware and injects it into the router state. If this fails the router
	// is instructed to navigate away from the route before the observer is complete.
	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Hardware> {
		var me = this;
		return new Observable( function( observer_: any ) {
			me.getHardware( +route_.params['hardware_id'] )
				.subscribe(
					function( hardware_: Hardware ) {
						observer_.next( hardware_ );
						observer_.complete();
					},
					function( error_: String ) {
						me._router.navigate( [ '/hardware' ] );
						observer_.next( null );
						observer_.complete();
					}
				)
			;
		} );
	}

	getHardwareList(): Observable<Hardware[]> {
		return this._http.get( this._hardwareUrlBase )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	getHardware( id_: Number ): Observable<Hardware> {
		return this._http.get( this._hardwareUrlBase + '/' + id_ )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	putHardware( hardware_: Hardware ): Observable<Hardware> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id, hardware_, options )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	private _extractData( response_: Response ) {
		let body = response_.json();
		return body || null;
	};

	private _handleError( response_: Response | any ) {
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

/*
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
*/

}
