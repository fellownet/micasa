import { Injectable } from '@angular/core';

@Injectable()

export class UserService {

	user: Object;

	doLogin( username: String, password: String ): void {
		this.user = { fullname: 'Bob Kersten (' + username + ')' };
	}
}
