import { Injectable }     from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import { SessionService } from '../session/session.service';
import { Setting }        from '../settings/settings.service';

export class Credentials {
	username: string;
	password: string;
	remember: boolean;
}

export enum ACL {
	Viewer = 1,
	User = 2,
	Installer = 3,
	Admin = 99
}

export class User {
	id: number;
	name: string;
	username: string;
	password?: string;
	rights: ACL;
	enabled: boolean;
	settings?: Setting[];
}

@Injectable()
export class UsersService {

	public lastPage: number = NaN;
	public returnUrl?: string;

	public constructor(
		private _sessionService: SessionService
	) {
	};

	public getUsers(): Observable<User[]> {
		return this._sessionService.http<User[]>( 'get', 'users' );
	};

	public getUser( id_: Number ): Observable<User> {
		return this._sessionService.http<User>( 'get', 'users/' + id_ );
	};

	public getUserSettings(): Observable<Setting[]> {
		let resource: string = 'users/settings';
		return this._sessionService.http<Setting[]>( 'get', resource );
	};

	public putUser( user_: User ): Observable<User> {
		let resource: string = 'users';
		if ( user_.id ) {
			return this._sessionService.http<User>( 'put', resource + '/' + user_.id, user_ );
		} else {
			return this._sessionService.http<User>( 'post', resource, user_ );
		}
	};

	public deleteUser( user_: User ): Observable<User> {
		let resource: string = 'users';
		return this._sessionService.http<any>( 'delete', resource + '/' + user_.id )
			.map( () => user_ )
		;
	};

}
