import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';
import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

export class Device {
	id: number;
	label: string;
	name: string;
	plugin: string;
	plugin_id: number;
	type: string;
	subtype: string;
	enabled: boolean;
	value?: any;
	unit: string;
	age: number;
	scripts?: number[];
	links?: number[];
	settings?: Setting[];
	readonly: boolean;
	total_scripts: number;
	total_timers: number;
	total_links: number;
	scheduled: boolean;
	battery_level?: number;
	signal_strength?: number;
}

@Injectable()
export class DevicesService {

	public lastPage: any = {};
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getDevices( pluginId_?: number ): Observable<Device[]> {
		let resource: string = 'devices';
		if ( !! pluginId_ ) {
			resource = 'plugins/' + pluginId_ + '/' + resource;
		}
		return this._sessionService.http<Device[]>( 'get', resource );
	};

	public getDevicesById( deviceIds_: number[] ): Observable<Device[]> {
		let resource: string = 'devices/' + deviceIds_.join( ',' );
		return this._sessionService.http<any>( 'get', resource )
			.map( devices_ => Array.isArray( devices_ ) ? devices_ : [ devices_ ] );
		;
	};

	public getDevice( id_: number ): Observable<Device> {
		return this._sessionService.http<Device>( 'get', 'devices/' + id_ );
	};

	public putDevice( device_: Device ): Observable<Device> {
		return this._sessionService.http<Device>( 'put', 'devices/' + device_.id, device_ );
	};

	public patchDevice( device_: Device, value_: any ): Observable<Device> {
		return this._sessionService.http<Device>( 'patch', 'devices/' + device_.id, {
			value: value_
		} );
	};

	public getData( id_: number, options_?: any ): Observable<any[]> {
		return this._sessionService.http<any[]>( 'post', 'devices/' + id_ + '/data', options_ );
	};

	public deleteDevice( device_: Device ): Observable<Device> {
		return this._sessionService.http<any>( 'delete', 'devices/' + device_.id )
			.map( () => device_ )
		;
	};

	public performAction( device_: Device ): Observable<Device> {
		var value: String = 'Activate';
		if ( device_.value == 'Disabled' ) {
			value = 'Enabled';
		}
		return this._sessionService.http<any>( 'patch', 'devices/' + device_.id, {
			value: value
		} );
	};

}
