import { Injectable }              from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                                  from '@angular/router';
import { Http, Response }          from '@angular/http';
import { Headers, RequestOptions } from '@angular/http';
import { Observable }              from 'rxjs/Observable';
import { Hardware }                from './hardware.service';

export class Device {
	id: number;
	label: string;
	name: string;
	hardware: Hardware;
	type: string;
	value: any;
}

@Injectable()
export class DeviceService implements Resolve<Device> {

	private _deviceUrlBase = 'api/devices';

	constructor( private _router: Router, private _http: Http ) { };

	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Device> {
		var me = this;
		return this.getDevice( +route_.params['id'] ).do( function( device_: Device ) {
			if ( device_ ) {
				return device_;
			} else {
				me._router.navigate( [ '/devices' ] );
				return null;
			}
		} );
	}

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

	getDevice( id_: number ): Observable<Device> {
		return this._http.get( this._deviceUrlBase + '/' + id_ )
			.map( this._extractData )
			.catch( this._handleError )
		;


		//return Observable.of( { id: id_, label: 'poekoe', name: 'plaap' } ).delay( 500 /* ms */ );
	};

}
