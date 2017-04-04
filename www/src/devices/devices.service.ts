import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';
import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

export class DataBundle {
	day?: any[];
	week?: any[];
	month?: any[];
	year?: any[];
}

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
	dataBundle?: DataBundle;
}

export class Link {
	id: number;
	name: string;
	device_id: number;
	device: Device|string;
	target_device_id: number;
	target_device: Device|string;
	value?: string;
	target_value?: string;
	after?: number;
	for?: number;
	clear?: boolean;
	enabled: boolean;
	settings?: Setting[];
}

@Injectable()
export class DevicesService {

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getDevices( hardwareId_?: number, scriptId_?: number, deviceIds_?: number[] ): Observable<Device[]> {
		let resource: string = 'devices';
		if ( hardwareId_ ) {
			resource += '?hardware_id=' + hardwareId_;
		} else if ( scriptId_ ) {
			resource += '?enabled=1&script_id=' + scriptId_;
		} else if ( deviceIds_ ) {
			resource += '?enabled=1&device_ids=' + deviceIds_.join( ',' );
		} else {
			resource += '?enabled=1';
		}
		return this._sessionService.http<Device[]>( 'get', resource );
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

	public deleteDevice( device_: Device ): Observable<boolean> {
		return this._sessionService.http<any>( 'delete', 'devices/' + device_.id )
			.map( function( result_: any ) {
				return true; // failures do not end up here
			} )
		;
	};

	public getLinks( deviceId_: number ): Observable<Link[]> {
		let resource: string = 'devices/' + deviceId_ + '/links';
		return this._sessionService.http<Link[]>( 'get', resource );
	};

	public getLink( deviceId_: number, linkId_: number ): Observable<Link> {
		let resource: string = 'devices/' + deviceId_ + '/links/' + linkId_;
		return this._sessionService.http<Link>( 'get', resource );
	};

	public getLinkSettings( deviceId_: number ): Observable<Setting[]> {
		let resource: string = 'devices/' + deviceId_ + '/links/settings';
		return this._sessionService.http<Setting[]>( 'get', resource );
	};

	public putLink( device_: Device, link_: Link ): Observable<Link> {
		let resource: string = 'devices/' + device_.id + '/links';
		if ( link_.id ) {
			return this._sessionService.http<Link>( 'put', resource + '/' + link_.id, link_ );
		} else {
			return this._sessionService.http<Link>( 'post', resource, link_ );
		}
	};

	public deleteLink( device_: Device, link_: Link ): Observable<boolean> {
		let resource: string = 'devices/' + device_.id + '/links';
		return this._sessionService.http<any>( 'delete', resource + '/' + link_.id )
			.map( function( result_: any ) {
				return true; // failures do not end up here
			} )
		;
	};
}
