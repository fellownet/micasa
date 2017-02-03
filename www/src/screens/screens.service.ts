import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
import {
	Device,
	DevicesService
}                         from '../devices/devices.service';

export class Widget {
	device_id: number;
	// TODO additional widget properties such as width or height
}

export class Screen {
	id: number;
	name: string;
	widgets: Widget[];
}

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
				return Observable.throw( 'invalid screen' );
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

}
