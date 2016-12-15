import { Injectable }              from '@angular/core';
import { Http, Response }          from '@angular/http';
import { Headers, RequestOptions } from '@angular/http';
import { Observable }              from 'rxjs/Observable';
import { Hardware }                from './hardware.service';

@Injectable()
export class Device {
	id: number;
	label: string;
	name: string;
	hardware: Hardware;
	type: string;
	value: any;
}

@Injectable()
export class DeviceService {

	private _deviceUrlBase = 'api/devices';

	constructor( private _http: Http ) { };

	getDevices(): Observable<Device[]> {
		return this._http.get( this._deviceUrlBase )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	putDevice( device_: Device ): Observable<Device[]> {
		let headers = new Headers( { 'Content-Type': 'application/json' } );
		let options = new RequestOptions( { headers: headers } );
		return this._http.put( this._deviceUrlBase + '/' + device_.id, device_, options )
			.map( this._extractData )
			.catch( this._handleError )
		;
	};

	private _extractData( response_: Response ): Device[] {
		let body = response_.json();
		return body || { };
	};

	private _handleError( response_: Response | any ) {
		let message: string;
		if ( response_ instanceof Response ) {
			message = 'an unspecified error to be specified later on';
		}
		return Observable.throw( message );
	};

}
