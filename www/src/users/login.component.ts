import {
	Component,
	OnInit
}                from '@angular/core';
import {
	Router
}                from '@angular/router';
import {
	Credentials,
	User,
	UsersService
}                from './users.service';

@Component( {
	templateUrl: 'tpl/login.html',
} )

export class LoginComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public credentials: Credentials = { username: "", password: "", remember: false };

	public constructor(
		private _router: Router,
		private _usersService: UsersService
	) {
	};

	public ngOnInit() {
		// This allows a redirect to /login to also be used for logging out.
		this._usersService.doLogout();
	};

	public doLogin(): void {
		var me = this;
		me.loading = true;
		me.error = null;
		this._usersService.doLogin( me.credentials )
			.subscribe(
				function( success_: boolean ) {
					if ( success_ ) {
						// The loginServive is also a route guard and has the route navigated to available which
						// we need to redirect to after the login.
						let url = me._usersService.redirectUrl ? me._usersService.redirectUrl : '/';
						me._router.navigate( [ url ] );
					} else {
						me.loading = false;
					}
				},
				function( error_: string ) {
					setTimeout( function() {
						me.loading = false;
						me.error = error_;
					}, 500 );
				}
			)
		;
	};
}
