import { Injectable } from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                     from '@angular/router';
import { Observable } from 'rxjs/Observable';

export const SCREENS: Screen[] = [
	{id: 1, name: 'Dashboard'},
	{id: 11, name: 'Temperatuur'},
	{id: 12, name: 'Energie'},
	{id: 13, name: 'Schakelaars'},
];

export class Screen {
	id: number;
	name: string;
}

@Injectable()
export class ScreenService implements Resolve<Screen> {

	constructor( private _router: Router ) {
	}

	resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Screen> {
		var me = this;
		let id = +( route_.params['id'] || 1 ); // shows the dashboard (id=1) if no valid id was present in the route
		return this.getScreen( id ).do( function( screen_: Screen ) {
			if ( screen_ ) {
				return screen_;
			} else if ( id != 1 ) {
				me._router.navigate( [ '/dashboard' ] );
				return null;
			} else {
				// dashboard removed? can't be.
				throw new Error( 'dashboard is missing' );
			}
		} );
	}

	getScreens(): Screen[] {
		return SCREENS;
	}

	getScreen( id_: number ): Observable<Screen> {
		if ( id_ == 13 ) {
			return Observable.of( null ).delay( 500 /* ms */ );
		} else {
			return Observable.of( { id: id_, name: 'poekoe' } ).delay( 500 /* ms */ );
		}
	};

}
