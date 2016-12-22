import { Component }             from '@angular/core';
import { LoginService }          from './login.service';

 // Is declared here for sibling children screen and menu because this is the common parent.
import { Screen, ScreenService } from './screen.service';

@Component( {
	selector: 'body',
	templateUrl: 'tpl/app.html',
	providers: [ ScreenService /* see not on import */ ]
} )

export class AppComponent {

	constructor( private _loginService: LoginService ) {

	}

	isLoggedIn(): boolean {
		return this._loginService.isLoggedIn();
	}

}
