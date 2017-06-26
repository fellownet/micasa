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
export class ScriptResolver implements Resolve<Script> {

	public constructor(
		private _router: Router,
		private _scriptsService: ScriptsService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Script> {
		let observable: Observable<Script>;
		if ( 'script_id' in route_.params ) {
			observable = this._scriptsService.getScript( route_.params['script_id'] );
		} else {
			observable = this._scriptsService.getScriptSettings()
				.mergeMap( settings_ => Observable.of( { id: NaN, name: 'New Script', code: '// enter code here', enabled: false, settings: settings_ } ) )
			;
		}
		return observable
			.catch( () => {
				this._router.navigate( [ '/error' ] );
				return Observable.of( null );
			} )
		;
	};

}
