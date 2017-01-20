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

export class Widget {
	deviceId: number;
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
		private _usersService: UsersService
	) {
	};

	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Screen> {
		var me = this;
		var settings = me._usersService.getLogin().settings;
		if ( route_.data['dashboard'] ) {
			return Observable.of( settings.screens[0] );
		} else if ( 'screen_index' in route_.params ) {
			if ( route_.params['screen_index'] < settings.screens.length ) {
				return Observable.of( settings.screens[route_.params['screen_index']] );
			} else {
				me._router.navigate( [ '/dashboard' ] );
				return Observable.throw( null );
			}
		} else {
			return Observable.of( { index: NaN, name: 'New Screen', widgets: [] } );
		}
	};

	getScreens(): Observable<Screen[]> {
		return Observable.of( this._usersService.getLogin().settings.screens );
	};

	putScreen( screen_: Screen ): Observable<Screen> {
		var me = this;
		var settings = me._usersService.getLogin().settings;
		if ( screen_.index ) {
			settings.screens[screen_.index] = screen_;
		} else {
			screen_.index = settings.screens.length;
			settings.screens[screen_.index] = screen_;
		}
		return new Observable<Screen>( function( subscriber_: any ) {
			me._usersService.syncSettings()
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
		for ( var index = 0; index < settings.screens.length; index++ ) {
			settings.screens[index].index = index;
		}

		return new Observable<boolean>( function( subscriber_: any ) {
			me._usersService.syncSettings()
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
