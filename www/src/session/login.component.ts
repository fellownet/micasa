import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router
}                  from '@angular/router';
import {
	Credentials,
	SessionService
}                  from './session.service';

@Component( {
	templateUrl: 'tpl/login.html'
} )

export class LoginComponent implements OnInit {

	public loading: boolean = false;
	public error: string;
	public credentials: Credentials = { username: "", password: "", remember: false };

	public constructor(
		private _router: Router,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		// This allows a redirect to /login to also be used for logging out.
		this._sessionService.logout();
	};

	public doLogin(): void {
		var me = this;
		me.loading = true;
		me.error = null; // clears any previous login failure errors

		me._sessionService.login( me.credentials )
			.subscribe(
				function( success_: boolean ) {
					if ( success_ ) {
						me._router.navigate( [ '/dashboard' ] );
					} else {
						me._router.navigate( [ '/login' ] );
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
