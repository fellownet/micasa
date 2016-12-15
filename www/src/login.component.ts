import { Component } from '@angular/core';
import { UserService } from './user.service';

@Component( {
	selector: 'login',
	templateUrl: 'tpl/login.html'
} )

export class LoginComponent  {

	constructor( private userService: UserService ) {

	}

	doLogin(): void {
		this.userService.doLogin( "wawa", "poepoe" );
	}

}
