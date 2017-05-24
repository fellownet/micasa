import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
import {
	Device,
	DevicesService
}                         from '../devices/devices.service';

export class Source {
	device_id: number;
	properties: any;
};

export class Widget {
	name: string;
	size: string;
	type: string;
	properties: any;
	sources: Source[];
	interval?: string;
	range?: number;
};

export class Screen {
	id: number;
	name?: string; // either device name or dedicated screen name
	device_id?: number;
	widgets: Widget[];
};

@Injectable()
export class ScreensService {

	public constructor(
		private _sessionService: SessionService,
		private _devicesService: DevicesService,
	) {
	};

	public getScreens(): Observable<Screen[]> {
		return this._sessionService.retrieve<Screen[]>( 'screens' );
	};

	public getScreen( id_: number ): Observable<Screen> {
		return this.getScreens()
			.map( function( screens_: Screen[] ) {
				for ( let screen of screens_ ) {
					if ( screen.id == id_ ) {
						return screen;
					}
				}
				throw new Error( 'invalid screen' );
			} )
		;
	};

	public getScreenForDevice( device_id_: number ): Observable<Screen> {
		return this.getScreens()
			.map( function( screens_: Screen[] ) {
				for ( let screen of screens_ ) {
					if ( screen.device_id == device_id_ ) {
						return screen;
					}
				}
				throw new Error( 'invalid screen' );
			} )
		;
	};

	public putScreen( screen_: Screen ): Observable<Screen[]> {
		var me = this;
		return me.getScreens()
			.mergeMap( function( screens_: Screen[] ) {
				let id: number = 0;
				for ( let i: number = 0; i < screens_.length; i++ ) {
					id = Math.max( id, screens_[i].id );
					if ( screens_[i].id == screen_.id ) {
						screens_[i] = screen_;
						return me._sessionService.store( 'screens', screens_ );
					}
				}
				screen_.id = ++id;
				screens_.push( screen_ );
				return me._sessionService.store( 'screens', screens_ );
			} )
		;
	};

	public deleteScreen( screen_: Screen ): Observable<Screen[]> {
		var me = this;
		return me.getScreens()
			.mergeMap( function( screens_: Screen[] ) {
				for ( let i: number = 0; i < screens_.length; i++ ) {
					if ( screens_[i].id == screen_.id ) {
						screens_.splice( i, 1 );
						break;
					}
				}
				return me._sessionService.store( 'screens', screens_ );
			} )
		;
	};

	public getDefaultScreenForDevice( device_: Device ) {
		let screen: Screen;
		switch( device_.type ) {
			case 'level':
			case 'counter':
				screen = {
					id: NaN,
					device_id: device_.id,
					widgets: [ {
						type: 'chart',
						name: 'Day',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue'
							}
						} ],
						interval: 'day',
						range: 1
					}, {
						type: 'chart',
						name: 'Week',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue'
							}
						} ],
						interval: 'week',
						range: 1
					}, {
						type: 'chart',
						name: 'Month',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue'
							}
						} ],
						interval: 'month',
						range: 1
					}, {
						type: 'chart',
						name: 'Year',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {
								color: 'blue'
							}
						} ],
						interval: 'year',
						range: 1
					} ]
				};
				break;

			case 'switch':
			case 'text':
				screen = {
					id: NaN,
					device_id: device_.id,
					widgets: [ {
						type: 'table',
						name: 'History',
						size: 'large',
						properties: {},
						sources: [ {
							device_id: device_.id,
							properties: {}
						} ],
						interval: 'week',
						range: 1
					} ]
				};
				break;

		}
		return screen;
	};

};
