import { Component, OnInit } from '@angular/core';
import { Router }            from '@angular/router';
import { LoginService }      from './login.service';

//http://jasonwatmore.com/post/2016/08/16/angular-2-jwt-authentication-example-tutorial

@Component( {
	selector: 'login',
	templateUrl: 'tpl/login.html'
} )

export class LoginComponent implements OnInit {

	loading: boolean = false;

	constructor( private _loginService: LoginService, public router: Router ) {
	}

	ngOnInit() {
		// This allows a redirect to /login to also be used for logging out.
		this._loginService.logout();
	};

	doLogin(): void {
		var me = this;
		me.loading = true;
		this._loginService.login( "wawa", "poepoe" )
			.subscribe( () => {
				me.loading = false;
				if ( me._loginService.isLoggedIn() ) {
					// The loginServive is also a route guard and has the route navigated to available which
					// we need to redirect to after the login.
					let url = me._loginService.redirectUrl ? me._loginService.redirectUrl : '/';
					me.router.navigate( [ url ] );
				}
			} );
		;
	}

	isLoggedIn(): boolean {
		return this._loginService.isLoggedIn();
	}

}
