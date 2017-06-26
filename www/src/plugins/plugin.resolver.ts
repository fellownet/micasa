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
export class PluginResolver implements Resolve<Plugin> {

	public constructor(
		private _router: Router,
		private _pluginsService: PluginsService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Plugin> {
		let observable: Observable<Plugin>;
		if ( 'plugin_id' in route_.params ) {
			observable = this._pluginsService.getPlugin( route_.params['plugin_id'] );
		} else {
			observable = this._pluginsService.getPluginSettings()
				.mergeMap( settings_ => Observable.of( { id: NaN, enabled: false, settings: settings_ } ) )
			;
		}
		return observable
			.catch( () => {
				this._router.navigate( [ '/error' ] );
				return Observable.of( null );
			} )
		;
	}

}
