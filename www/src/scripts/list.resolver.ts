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
		return this._scriptsService.getScripts()
			.catch( () => {
				this._router.navigate( [ '/error' ] );
				return Observable.of( null );
			} )
		;
	};

}
