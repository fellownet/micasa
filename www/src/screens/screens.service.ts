import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import {
	Http,
	Response,
	Headers,
	RequestOptions
}                          from '@angular/http';
import { Observable }      from 'rxjs/Observable';

import { UsersService }    from '../users/users.service';
import {
	Device,
	DevicesService
}                          from '../devices/devices.service';

declare var Highcharts: any;

export class Widget {
	device_id: number;
	device?: Device;
}

export class Screen {
	index: number;
	name: string;
	widgets: Widget[];
}

@Injectable()
export class ScreensService implements Resolve<Screen> {

	constructor(
		private _router: Router,
		private _http: Http,
		private _usersService: UsersService,
		private _devicesService: DevicesService
	) {
	};

	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Screen> {
		var me = this;
		var settings = me._usersService.getLogin().settings;
		let screen: Screen;

		if ( route_.data['dashboard'] ) {
			screen = settings.screens[0];
		} else if ( 'screen_index' in route_.params ) {
			if ( route_.params['screen_index'] < settings.screens.length ) {
				screen = settings.screens[route_.params['screen_index']];
			} else {
				me._router.navigate( [ '/dashboard' ] );
				return Observable.throw( null );
			}
		} else {
			return Observable.of( { index: NaN, name: 'New Screen', widgets: [] } );
		}

		// Before navigating to a particular screen we need to fetch all the devices on that screen,
		// which are them combined with the widget config.
		let widgetIds: Array<number> = [];
		for ( var i = 0; i < screen.widgets.length; i++ ) {
			widgetIds.push( screen.widgets[i].device_id );
		}
		return new Observable( function( subscriber_: any ) {
			me._devicesService.getDevices( undefined, undefined, widgetIds )
				.subscribe(
					function( devices_: Device[] ) {

						// Then we need to merge the widgets with the fetched device data for a complete
						// screen object to be passed to the screen component.
						for ( var i = 0; i < screen.widgets.length; i++ ) {
							for ( var j = 0; j < devices_.length; j++ ) {
								if ( screen.widgets[i].device_id == devices_[j].id ) {
									screen.widgets[i].device = devices_[j];
									break;
								}
							}
						}

						subscriber_.next( screen );
						subscriber_.complete();
					},
					function( error_: string ) {
						me._router.navigate( [ '/login' ] ); // there's no alternative to route to
						subscriber_.next( null );
						subscriber_.complete();
					}
				)
			;
		} );
	};

	getScreens(): Observable<Screen[]> {
		return Observable.of( this._usersService.getLogin().settings.screens );
	};

	putScreen( screen_: Screen ): Observable<Screen> {
		// TODO only update the screen that was provided. Might require some changes to the callback
		// handler on de server side.
		var me = this;
		var settings = me._usersService.getLogin().settings;
		if (
			'index' in screen_
			&& ! isNaN( screen_.index )
		) {
			settings.screens[screen_.index] = screen_;
		} else {
			screen_.index = settings.screens.length;
			settings.screens[screen_.index] = screen_;
		}
		return new Observable<Screen>( function( subscriber_: any ) {
			me._usersService.syncSettings( 'screens' )
				.subscribe(
					function( result_: any ) {
						subscriber_.next( screen_ );
						subscriber_.complete();
					},
					function( error_: string ) {
						subscriber_.error( error_ );
						subscriber_.complete();
					}
				)
			;
		} );
	};

	deleteScreen( screen_: Screen ): Observable<boolean> {
		var me = this;
		var settings = me._usersService.getLogin().settings;
		settings.screens.splice( screen_.index, 1 );
		// NOTE resetting indexes on *all* screens, not just the ones after the removed screen.
		for ( var index = 0; index < settings.screens.length; index++ ) {
			settings.screens[index].index = index;
		}

		return new Observable<boolean>( function( subscriber_: any ) {
			me._usersService.syncSettings( 'screens' )
				.subscribe(
					function( result_: any ) {
						subscriber_.next( true );
						subscriber_.complete();
					},
					function( error_: string ) {
						subscriber_.error( error_ );
						subscriber_.complete();
					}
				)
			;
		} );
	};

}
