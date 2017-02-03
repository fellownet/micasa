import { Injectable }      from '@angular/core';
import { Observable }      from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import {
	Session,
	SessionService
}                          from '../session/session.service';
import {
	Device,
	DevicesService
}                          from '../devices/devices.service';

export class Widget {
	device_id: number;
	width: number;
	height: number;
}

export class Screen {
	id: number;
	name: string;
	widgets: Widget[];
}

@Injectable()
export class ScreensService {

	private _screens: BehaviorSubject<Screen[]> = new BehaviorSubject( [] );

	public screens: Observable<Screen[]> = this._screens.asObservable();

	public constructor(
		private _sessionService: SessionService,
		private _devicesService: DevicesService
	) {
		var me = this;
		_sessionService.session.subscribe(
			function( session_: Session ) {
				if ( session_ == null ) {
					me._screens.next( [] );
				} else {
//					me.getScreens().subscribe(
//						function( screens_: Screen[] ) {
//						}
//					);
				}
			} )
		;
	};

	public getScreens(): Observable<Screen[]> {
		var me = this;
		return me._sessionService.retrieve<Screen[]>( 'screens' )
			.do( function( screens_: Screen[] ) {
				me._screens.next( screens_ );
			} )
			.catch( function( error_: string ) {

				// The server doesn't appear to have a screens state yet, so we're creating
				// a single dashboard screen here.
				let screens: Screen[] = [ { id: 1, name: 'Dashboard', widgets: [] } ];
				me._screens.next( screens );

				// The new list of screens is immediately pushed to the server to create the
				// screens state property.
				return me._sessionService.store( 'screens', screens )
					.map( function( success_: boolean ) {
						return screens;
					} )
				;
			} )
		;
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

	public putScreen( screen_: Screen ): Observable<boolean> {
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

				me._screens.next( screens_ );
				return me._sessionService.store( 'screens', screens_ );
			} )
		;
	};

	public deleteScreen( screen_: Screen ): Observable<boolean> {
		var me = this;
		return me.getScreens()
			.mergeMap( function( screens_: Screen[] ) {
				for ( let i: number = 0; i < screens_.length; i++ ) {
					if ( screens_[i].id == screen_.id ) {
						screens_.splice( i, 1 );
						break;
					}
				}

				me._screens.next( screens_ );
				return me._sessionService.store( 'screens', screens_ );
			} )
		;
	};

	public addDeviceToScreen( screen_: Screen, device_: Device ): Observable<boolean> {
		return Observable.throw( "Neee nie goed" );
	};
}
