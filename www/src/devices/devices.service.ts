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
import { UsersService }    from '../users/users.service';

export class Option {
	label: string;
	value: string;
}

export class Setting {
	label: string;
	description?: string;
	name: string;
	type: string;
	class: string;
	value?: any;
	min?: number;
	max?: number;
	options?: Option[];
}

export class Device {
	id: number;
	label: string;
	name: string;
	hardware: Hardware;
	type: string;
	subtype: string;
	enabled: boolean;
	value?: any; // is optional for updates
	unit: string;
	scripts?: number[];
	settings?: Setting[];
}

@Injectable()
export class DevicesService implements Resolve<Device> {

	private _deviceUrlBase = 'api/devices';

	constructor(
		private _router: Router,
		private _http: Http,
		private _usersService: UsersService
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
		} else {
			uri += '?enabled=1';
		}
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( uri, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getDevice( id_: Number ): Observable<Device> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._deviceUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	putDevice( device_: Device, updateValue_: boolean = false ): Observable<Device> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		// Value should not be sent along with the update to prevent server warnings for
		// invalid update source.
		let data: Device = Object.assign( {}, device_ );
		if ( ! updateValue_ ) {
			delete( data.value );
		}
		return this._http.put( this._deviceUrlBase + '/' + device_.id, data, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getData( device_: Device ): Observable<any[]> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._deviceUrlBase + '/' + device_.id + '/data', options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	deleteDevice( device_: Device ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._deviceUrlBase + '/' + device_.id, options )
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
