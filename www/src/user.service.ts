import { Injectable } from '@angular/core';

@Injectable()
export class User {

}

@Injectable()
export class UserService {

	user: Object;

	doLogin( username: string, password: string ): void {
		this.user = { fullname: 'Bob Kersten (' + username + ')' };
	}
}
