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

export class Script {
	id: number;
	name: string;
	code?: string;
	runs?: number;
	enabled: boolean;
}

@Injectable()
export class ScriptsService {

	private _scriptUrlBase = 'api/scripts';

	constructor(
		private _router: Router,
		private _http: Http
	) {
	};

	// The resolve method gets executed by the router before a route is being navigated to. This
	// method fetches the script and injects it into the router state. If this fails the router
	// is instructed to navigate away from the route before the observer is complete.
	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Script> {
		var me = this;
		if ( route_.params['script_id'] == 'add' ) {
			return Observable.of( { id: NaN, name: 'New script', code: '// enter code here', enabled: true } );
		} else {
			return new Observable( function( observer_: any ) {
				me.getScript( +route_.params['script_id'] )
					.subscribe(
						function( script_: Script ) {
							observer_.next( script_ );
							observer_.complete();
						},
						function( error_: string ) {
							me._router.navigate( [ '/scripts' ] );
							observer_.next( null );
							observer_.complete();
						}
					)
				;
			} );
		}
	}

	getScripts(): Observable<Script[]> {
		return this._http.get( this._scriptUrlBase )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getScript( id_: Number ): Observable<Script> {
		return this._http.get( this._scriptUrlBase + '/' + id_ )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	putScript( script_: Script ): Observable<Script> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
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

	deleteScript( script_: Script ): Observable<boolean> {
		return this._http.delete( this._scriptUrlBase + '/' + script_.id )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	private _extractData( response_: Response ) {
		let body = response_.json();
		return body || null;
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
