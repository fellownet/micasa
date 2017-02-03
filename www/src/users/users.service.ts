import { Injectable }      from '@angular/core';
import { Observable }      from 'rxjs/Observable';

import { SessionService }  from '../session/session.service';

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
}

@Injectable()
export class UsersService {

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

	public putUser( user_: User ): Observable<User> {
		if ( user_.id ) {
			return this._sessionService.http<User>( 'put', 'users' + '/' + user_.id, user_ );
		} else {
			return this._sessionService.http<User>( 'post', 'users', user_ );
		}
	};

	public deleteUser( user_: User ): Observable<boolean> {
		return this._sessionService.http<any>( 'delete', 'users/' + user_.id )
			.map( function( result_: any ) {
				return true; // failures do not end up here
			} )
		;
	};
}
