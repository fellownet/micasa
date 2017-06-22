import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	Plugin,
	PluginsService
}                          from './plugins.service';

@Injectable()
export class PluginsListResolver implements Resolve<Plugin[]> {

	public constructor(
		private _router: Router,
		private _pluginsService: PluginsService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Plugin[]> {
		return this._pluginsService.getList()
			.catch( () => {
				this._router.navigate( [ '/error' ] );
				return Observable.of( null );
			} )
		;
	}

}
