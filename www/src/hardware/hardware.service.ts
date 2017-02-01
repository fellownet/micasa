import { Injectable }      from '@angular/core';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                          from '@angular/http';
import { Observable }      from 'rxjs/Observable';
import { Device }          from '../devices/devices.service';
import { UsersService }    from '../users/users.service';
import { Setting }         from '../settings/settings.service';

export class Hardware {
	id: number;
	label: string;
	name: string;
	type: string;
	enabled: boolean;
	state: string;
	last_update: number;
	parent?: Hardware;
	settings?: Setting[];
}

@Injectable()
export class HardwareService {

	private _hardwareUrlBase = 'api/hardware';
	private _devicesUrlBase = 'api/devices';

	public constructor(
		private _http: Http,
		private _usersService: UsersService
	) {
	};

	public getList( hardwareId_?: Hardware ): Observable<Hardware[]> {
		let uri: string = this._hardwareUrlBase;
		if ( hardwareId_ ) {
			uri += '?hardware_id=' + hardwareId_;
		}
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( uri, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public getHardware( id_: Number ): Observable<Hardware> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._hardwareUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public putHardware( hardware_: Hardware ): Observable<Hardware> {
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

	public addHardware( type_: string ): Observable<Hardware> {
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

	public deleteHardware( hardware_: Hardware ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.delete( this._hardwareUrlBase + '/' + hardware_.id, options )
			.map( function( response_: Response ) {
				return response_.json()['result'] == 'OK';
			} )
			.catch( this._handleHttpError )
		;
	};

	public performAction( hardware_: Hardware, device_: Device ): Observable<boolean> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.patch( this._devicesUrlBase + '/' + device_.id, { value: 'Activate' }, options )
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
