import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';
import {
	Hardware,
	HardwareService
}                          from './hardware.service';

@Injectable()
export class HardwareResolver implements Resolve<Hardware> {

	public constructor(
		private _router: Router,
		private _hardwareService: HardwareService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<Hardware> {
		return this._hardwareService.getHardware( route_.params['hardware_id'] )
			.catch( () => {
				this._router.navigate( [ '/error' ] );
				return Observable.of( null );
			} )
		;
	}

}
