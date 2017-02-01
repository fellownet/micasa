import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import {
	Script,
	ScriptsService
}                          from './scripts.service';

@Injectable()
export class ScriptsListResolver implements Resolve<Script[]> {

	public constructor(
		private _router: Router,
		private _scriptsService: ScriptsService
	) {
	};

	// The resolve method gets executed by the router before a route is being navigated to. This
	// method fetches the script and injects it into the router state. If this fails the router
	// is instructed to navigate away from the route before the observer is complete.
	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Script[]> {
		var me = this;
		return new Observable( function( subscriber_: any ) {
			me._scriptsService.getScripts()
				.subscribe(
					function( scripts_: Script[] ) {
						subscriber_.next( scripts_ );
						subscriber_.complete();
					},
					function( error_: string ) {
						me._router.navigate( [ '/login' ] );
						subscriber_.next( null );
						subscriber_.complete();
					}
				)
			;
		} );
	};
}
