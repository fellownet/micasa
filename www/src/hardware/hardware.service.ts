import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';
import { Device }         from '../devices/devices.service';
import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

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

	public lastPage: any = {};
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getList( hardwareId_?: Hardware ): Observable<Hardware[]> {
		let resource: string = 'hardware';
		if ( hardwareId_ ) {
			resource += '?hardware_id=' + hardwareId_;
		}
		return this._sessionService.http<Hardware[]>( 'get', resource );
	};

	public getHardware( id_: Number ): Observable<Hardware> {
		return this._sessionService.http<Hardware>( 'get', 'hardware/' + id_ );
	};

	public putHardware( hardware_: Hardware ): Observable<Hardware> {
		return this._sessionService.http<Hardware>( 'put', 'hardware/' + hardware_.id, hardware_ );
	};

	public addHardware( type_: string ): Observable<Hardware> {
		return this._sessionService.http<Hardware>( 'post', 'hardware', {
			type: type_,
			name: 'New Hardware'
		} );
	};

	public deleteHardware( hardware_: Hardware ): Observable<Hardware> {
		return this._sessionService.http<any>( 'delete', 'hardware/' + hardware_.id )
			.map( () => hardware_ )
		;
	};

}
