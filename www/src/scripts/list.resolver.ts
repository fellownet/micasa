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

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Script[]> {
		var me = this;
		return this._scriptsService.getScripts()
			.catch( function( error_: string ) {
				me._router.navigate( [ '/login' ] );
				return Observable.of( null );
			} )
		;
	};
}
