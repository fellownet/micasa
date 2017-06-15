import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';
import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';
import { Device }         from '../devices/devices.service';

export class Link {
	id: number;
	name: string;
	device_id: number;
	device: Device|string;
	target_device_id: number;
	target_device: Device|string;
	value?: string;
	target_value?: string;
	after?: number;
	for?: number;
	clear?: boolean;
	enabled: boolean;
	settings?: Setting[];
}

@Injectable()
export class LinksService {

	public lastPage: any = {};
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getLinks( device_id_?: number ): Observable<Link[]> {
		let resource: string = 'links';
		if ( device_id_ ) {
			resource += '?device_id=' + device_id_;
		}
		return this._sessionService.http<Link[]>( 'get', resource );
	};

	public getLink( link_id_: number ): Observable<Link> {
		let resource: string = 'links/' + link_id_;
		return this._sessionService.http<Link>( 'get', resource );
	};

	public getLinkSettings(): Observable<Setting[]> {
		let resource: string = 'links/settings';
		return this._sessionService.http<Setting[]>( 'get', resource );
	};

	public putLink( link_: Link ): Observable<Link> {
		let resource: string = 'links';
		if ( link_.id ) {
			return this._sessionService.http<Link>( 'put', resource + '/' + link_.id, link_ );
		} else {
			return this._sessionService.http<Link>( 'post', resource, link_ );
		}
	};

	public deleteLink( link_: Link ): Observable<Link> {
		let resource: string = 'links';
		return this._sessionService.http<any>( 'delete', resource + '/' + link_.id )
			.map( () => link_ )
		;
	};

}
