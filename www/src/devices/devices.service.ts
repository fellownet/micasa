import { Injectable }      from '@angular/core';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                          from '@angular/http';
import { Observable }      from 'rxjs/Observable';
import { UsersService }    from '../users/users.service';
import { Setting }         from '../settings/settings.service';

export class Device {
	id: number;
	label: string;
	name: string;
	hardware: string;
	hardware_id: number;
	type: string;
	subtype: string;
	enabled: boolean;
	value?: any;
	unit: string;
	last_update: number;
	scripts?: number[];
	settings?: Setting[];
	readonly: boolean;
	total_scripts: number;
	total_timers: number;
}

@Injectable()
export class DevicesService {

	private _deviceUrlBase = 'api/devices';

	public constructor(
		private _http: Http,
		private _usersService: UsersService
	) {
	};

	public getDevices( hardwareId_?: number, scriptId_?: number, deviceIds_?: number[] ): Observable<Device[]> {
		let uri: string = this._deviceUrlBase;
		if ( hardwareId_ ) {
			uri += '?hardware_id=' + hardwareId_;
		} else if ( scriptId_ ) {
			uri += '?enabled=1&script_id=' + scriptId_;
		} else if ( deviceIds_ ) {
			uri += '?enabled=1&device_ids=' + deviceIds_.join( ',' );
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

	public getDevice( id_: number ): Observable<Device> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._deviceUrlBase + '/' + id_, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public putDevice( device_: Device ): Observable<Device> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		let data: Device = Object.assign( {}, device_ );
		return this._http.put( this._deviceUrlBase + '/' + device_.id, data, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public patchDevice( device_: Device, value_: any ): Observable<Device> {
		let headers = new Headers( {
			'Content-Type'  : 'application/json',
			'Authorization' : this._usersService.getLogin().token
		} );
		let options = new RequestOptions( { headers: headers } );
		return this._http.patch( this._deviceUrlBase + '/' + device_.id, { value: value_ }, options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public getData( id_: number ): Observable<any[]> {
		let headers = new Headers( { 'Authorization': this._usersService.getLogin().token } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.get( this._deviceUrlBase + '/' + id_ + '/data', options )
			.map( this._extractData )
			.catch( this._handleHttpError )
		;
	};

	public deleteDevice( device_: Device ): Observable<boolean> {
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
