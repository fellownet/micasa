import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

export class Timer {
	id: number;
	name: string;
	cron: string;
	enabled: boolean;
	scripts?: number[];
	device_id?: number;
	value?: string;
	settings?: Setting[];
}

@Injectable()
export class TimersService {

	public lastPage: any = {};
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getTimers( device_id_?: number ): Observable<Timer[]> {
		let resource: string = 'timers';
		if ( !! device_id_ ) {
			resource = 'devices/' + device_id_ + '/' + resource;
		}
		return this._sessionService.http<Timer[]>( 'get', resource );
	};

	public getTimer( id_: number, device_id_?: number ): Observable<Timer> {
		let resource: string = 'timers/' + id_;
		if ( !! device_id_ ) {
			resource = 'devices/' + device_id_ + '/' + resource;
		}
		return this._sessionService.http<Timer>( 'get', resource );
	};

	public getTimerSettings( device_id_?: number ): Observable<Setting[]> {
		let resource: string = 'timers/settings';
		if ( !! device_id_ ) {
			resource = 'devices/' + device_id_ + '/' + resource;
		}
		return this._sessionService.http<Setting[]>( 'get', resource );
	};

	public putTimer( timer_: Timer, device_id_?: number ): Observable<Timer> {
		let resource: string = 'timers';
		if ( !! device_id_ ) {
			resource = 'devices/' + device_id_ + '/' + resource;
		}
		if ( timer_.id ) {
			return this._sessionService.http<Timer>( 'put', resource + '/' + timer_.id, timer_ );
		} else {
			return this._sessionService.http<Timer>( 'post', resource, timer_ );
		}
	};

	public deleteTimer( timer_: Timer, device_id_?: number ): Observable<Timer> {
		let resource: string = 'timers';
		if ( !! device_id_ ) {
			resource = 'devices/' + device_id_ + '/' + resource;
		}
		return this._sessionService.http<any>( 'delete', resource + '/' + timer_.id )
			.map( () => timer_ )
		;
	};

}
