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
import { Hardware }        from '../hardware/hardware.service';

export class Device {
	id: number;
	label: string;
	name: string;
	hardware: Hardware;
	type: string;
	value: any;
}

@Injectable()
export class DevicesService implements Resolve<Device> {

	private _deviceUrlBase = 'api/devices';

	constructor(
		private _router: Router,
		private _http: Http
	) {
	};

	// The resolve method gets executed by the router before a route is being navigated to. This
	// method fetches the device and injects it into the router state. If this fails the router
	// is instructed to navigate away from the route before the observer is complete.
	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Device> {
		var me = this;
		return new Observable( function( observer_: any ) {
			me.getDevice( +route_.params['device_id'] )
				.subscribe(
					function( device_: Device ) {

						// If a hardware id was provided (when coming from the hardware details
						// component) it should match the one returned in device.
						if (
							route_.params['hardware_id']
							&& +route_.params['hardware_id'] != device_.hardware.id
						) {
							me._router.navigate( [ '/hardware' ] );
							observer_.next( null );
						} else {
							observer_.next( device_ );
						}
						observer_.complete();
					},
					function( error_: string ) {
						me._router.navigate( [ '/devices' ] );
						observer_.next( null );
						observer_.complete();
					}
				)
			;
		} );
	}

	getDevices( hardware_?: Hardware ): Observable<Device[]> {
		let uri: string = this._deviceUrlBase;
		if ( hardware_ ) {
			uri += '?hardware_id=' + hardware_.id;
		}
		return this._http.get( uri )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	getDevice( id_: Number ): Observable<Device> {
		return this._http.get( this._deviceUrlBase + '/' + id_ )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	putDevice( device_: Device ): Observable<Device> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._deviceUrlBase + '/' + device_.id, device_, options )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	getData( device_: Device ): Observable<any[]> {
		return this._http.get( this._deviceUrlBase + '/' + device_.id + '/data' )
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

}
