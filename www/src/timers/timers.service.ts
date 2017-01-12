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
import { UsersService }    from '../users/users.service';

export class Timer {
	id: number;
	name: string;
	cron: string;
	enabled: boolean;
	scripts: number[];
}

@Injectable()
export class TimersService {

	private _timerUrlBase = 'api/timers';

	constructor(
		private _router: Router,
		private _http: Http,
		private _usersService: UsersService
	) {
	};

	// The resolve method gets executed by the router before a route is being navigated to. This
	// method fetches the timer and injects it into the router state. If this fails the router
	// is instructed to navigate away from the route before the observer is complete.
	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Timer> {
		var me = this;
		if ( route_.params['timer_id'] == 'add' ) {
			return Observable.of( { id: NaN, name: 'New timer', cron: '* * * * *', enabled: false, scripts: [] } );
		} else {
			return new Observable( function( observer_: any ) {
				me.getTimer( +route_.params['timer_id'] )
					.subscribe(
						function( timer_: Timer ) {
							observer_.next( timer_ );
							observer_.complete();
						},
						function( error_: string ) {
							me._router.navigate( [ '/timers' ] );
							observer_.next( null );
							observer_.complete();
						}
					)
				;
			} );
		}
	}

	getTimers(): Observable<Timer[]> {
		let headers = new Headers( { 'Authorization': this._usersService.getLoggedInToken() } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._timerUrlBase, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getTimer( id_: Number ): Observable<Timer> {
		let headers = new Headers( { 'Authorization': this._usersService.getLoggedInToken() } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._timerUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	putTimer( timer_: Timer ): Observable<Timer> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLoggedInToken()
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

	deleteTimer( timer_: Timer ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLoggedInToken() } );
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
