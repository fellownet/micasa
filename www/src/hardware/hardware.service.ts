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
import { Device }          from '../devices/devices.service';
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

export class Hardware {
	id: number;
	label: string;
	name: string;
	type: string;
	enabled: boolean;
	state: string;
	parent?: Hardware;
	settings?: Setting[];
}

@Injectable()
export class HardwareService {

	private _hardwareUrlBase = 'api/hardware';

	constructor(
		private _router: Router,
		private _http: Http,
		private _usersService: UsersService
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
	};

	getHardwareList( parent_?: Hardware ): Observable<Hardware[]> {
		let uri: string = this._hardwareUrlBase;
		if ( parent_ ) {
			uri += '?hardware_id=' + parent_.id;
		}
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( uri, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	getHardware( id_: Number ): Observable<Hardware> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._hardwareUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	putHardware( hardware_: Hardware ): Observable<Hardware> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id, hardware_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	addHardware( type_: string ): Observable<Hardware> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		let data: any = {
			type: type_,
			name: 'New Hardware'
		};
		return this._http.post( this._hardwareUrlBase, data, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	deleteHardware( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._hardwareUrlBase + '/' + hardware_.id, options )
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

	zwaveIncludeMode( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id + '/include', {}, options )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	exitZWaveIncludeMode( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._hardwareUrlBase + '/' + hardware_.id + '/include', options )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	zwaveExcludeMode( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id + '/exclude', {}, options )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	exitZWaveExcludeMode( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._hardwareUrlBase + '/' + hardware_.id + '/exclude', options )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	zwaveHealNetwork( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id + '/heal', {}, options )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	zwaveHealNode( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._hardwareUrlBase + '/' + hardware_.id + '/heal', {}, options )
			.map( function( response_: Response ) {
					return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	dummyAddDevice( hardware_: Hardware, type_: string ): Observable<Device> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		let data: any = {
			type: type_,
			name: 'New Dummy Device'
		};
		return this._http.post( this._hardwareUrlBase + '/' + hardware_.id, data, options )
			.map( function( response_: Response ) {
				let body = response_.json();
				if ( body && body.device ) {
					return body.device;
				}
				return null;
			} )
			.catch( this._handleHttpError )
		;
	};

}
