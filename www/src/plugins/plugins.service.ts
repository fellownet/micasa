import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';
import { Device }         from '../devices/devices.service';
import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

export class Plugin {
	id: number;
	name?: string;
	enabled: boolean;
	label?: string;
	type?: string;
	state?: string;
	parent?: Plugin;
	children?: Plugin[];
	settings?: Setting[];
}

@Injectable()
export class PluginsService {

	public lastPage: any = {};
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getList(): Observable<Plugin[]> {
		let resource: string = 'plugins';
		return this._sessionService.http<Plugin[]>( 'get', resource );
	};

	public getPlugin( id_: Number ): Observable<Plugin> {
		return this._sessionService.http<Plugin>( 'get', 'plugins/' + id_ );
	};

	public getPluginSettings(): Observable<Setting[]> {
		let resource: string = 'plugins/settings';
		return this._sessionService.http<Setting[]>( 'get', resource );
	};

	public putPlugin( plugin_: Plugin ): Observable<Plugin> {
		if ( !! plugin_.id ) {
			return this._sessionService.http<Plugin>( 'put', 'plugins/' + plugin_.id, plugin_ );
		} else {
			return this._sessionService.http<Plugin>( 'post', 'plugins', plugin_ );
		}
	};

	public deletePlugin( plugin_: Plugin ): Observable<Plugin> {
		return this._sessionService.http<any>( 'delete', 'plugins/' + plugin_.id )
			.map( () => plugin_ )
		;
	};

}
