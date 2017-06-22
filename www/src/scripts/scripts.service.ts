import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

export class Script {
	id: number;
	name: string;
	code?: string;
	device_ids?: number[];
	enabled: boolean;
	settings?: Setting[];
}

@Injectable()
export class ScriptsService {

	public lastPage: number = NaN;
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getScripts(): Observable<Script[]> {
		return this._sessionService.http<Script[]>( 'get', 'scripts' );
	};

	public getScript( id_: Number ): Observable<Script> {
		return this._sessionService.http<Script>( 'get', 'scripts' + '/' + id_ )
			.map( script_ => {
				script_.settings.every( function( setting_: Setting ) {
					if ( setting_.name == 'code' ) {
						setting_.type = 'hidden';
						return false;
					}
					return true;
				} )
				return script_;
			} )
		;
	};

	public getScriptSettings(): Observable<Setting[]> {
		let resource: string = 'scripts/settings';
		return this._sessionService.http<Setting[]>( 'get', resource )
			.map( settings_ => {
				settings_.every( function( setting_: Setting ) {
					if ( setting_.name == 'code' ) {
						setting_.type = 'hidden';
						return false;
					}
					return true;
				} )
				return settings_;
			} )
		;
	};

	public putScript( script_: Script ): Observable<Script> {
		if ( script_.id ) {
			return this._sessionService.http<Script>( 'put', 'scripts' + '/' + script_.id, script_ );
		} else {
			return this._sessionService.http<Script>( 'post', 'scripts', script_ );
		}
	};

	public deleteScript( script_: Script ): Observable<Script> {
		return this._sessionService.http<any>( 'delete', 'scripts/' + script_.id )
			.map( () => script_ )
		;
	};

}
