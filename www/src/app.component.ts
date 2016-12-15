import { Component } from '@angular/core';
import { AppService } from './app.service';
import { UserService } from './user.service';

@Component( {
	selector: 'body',
	templateUrl: 'tpl/app.html',
	providers: [ UserService, AppService ]
} )

export class AppComponent {

	constructor( private _appService: AppService, private _userService: UserService ) {

	}

	isLoggedIn(): boolean {
		return this._userService.user != null;
	}

	getActiveSection(): string {
		return this._appService.activeSection;
	}

}
