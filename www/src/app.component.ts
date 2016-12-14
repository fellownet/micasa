import { Component } from '@angular/core';
import { UserService } from './user.service';

@Component( {
	selector: 'body',
	templateUrl: 'tpl/app.html',
	providers: [ UserService ]
} )

export class AppComponent {

	constructor( private userService: UserService ) {

	}

	isLoggedIn(): boolean {
		return this.userService.user != null;
	}

}
