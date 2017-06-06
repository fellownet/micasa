import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';

export class Timer {
	id: number;
	name: string;
	cron: string;
	enabled: boolean;
	scripts?: number[];
	device_id?: number;
	value?: string;
}

@Injectable()
export class TimersService {

	public lastPage: any = {};
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getTimers( deviceId_?: number ): Observable<Timer[]> {
		let resource: string = 'timers';
		if ( deviceId_ ) {
			resource += '?device_id=' + deviceId_;
		}
		return this._sessionService.http<Timer[]>( 'get', resource );
	};

	public getTimer( id_: number, deviceId_?: number ): Observable<Timer> {
		let resource: string = 'timers/' + id_;
		if ( deviceId_ ) {
			resource += '?device_id=' + deviceId_;
		}
		return this._sessionService.http<Timer>( 'get', resource );
	};

	public putTimer( timer_: Timer ): Observable<Timer> {
		if ( timer_.id ) {
			return this._sessionService.http<Timer>( 'put', 'timers' + '/' + timer_.id, timer_ );
		} else {
			return this._sessionService.http<Timer>( 'post', 'timers', timer_ );
		}
	};

	public deleteTimer( timer_: Timer ): Observable<Timer> {
		return this._sessionService.http<any>( 'delete', 'timers/' + timer_.id )
			.map( function( result_: any ) {
				return timer_;
			} )
		;
	};
}
