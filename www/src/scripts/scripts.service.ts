import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';

export class Script {
	id: number;
	name: string;
	code?: string;
	device_ids?: number[];
	enabled: boolean;
}

@Injectable()
export class ScriptsService {

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getScripts(): Observable<Script[]> {
		return this._sessionService.http<Script[]>( 'get', 'scripts' );
	};

	public getScript( id_: Number ): Observable<Script> {
		return this._sessionService.http<Script>( 'get', 'scripts' + '/' + id_ );
	};

	public putScript( script_: Script ): Observable<Script> {
		if ( script_.id ) {
			return this._sessionService.http<Script>( 'put', 'scripts' + '/' + script_.id, script_ );
		} else {
			return this._sessionService.http<Script>( 'post', 'scripts', script_ );
		}
	};

	public deleteScript( script_: Script ): Observable<boolean> {
		return this._sessionService.http<any>( 'delete', 'scripts/' + script_.id )
			.map( function( result_: any ) {
				return true; // failures do not end up here
			} )
		;
	};
}
