import { Component }    from '@angular/core';
import { UsersService } from './users/users.service';

@Component( {
	selector    : 'body',
	templateUrl : 'tpl/app.html'
} )

export class AppComponent {

	constructor( private _usersService: UsersService ) {
	};

	isLoggedIn(): boolean {
		return this._usersService.isLoggedIn();
	};

}
