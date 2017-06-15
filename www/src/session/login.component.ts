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

	public error: string;
	public visible: boolean;
	public credentials: Credentials = { username: "", password: "", remember: false };

	public constructor(
		private _router: Router,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		// This allows a redirect to /login to also be used for logging out.
		this._sessionService.logout();
		this.visible = true;
	};

	public doLogin(): void {
		this.error = null; // clears any previous login failure errors
		this._sessionService.login( this.credentials )
			.subscribe(
				session_ => {
					// The dashboard might take a while to load by the resolver, so hide the login window immediately.
					this.visible = false;
					this._router.navigate( [ '/dashboard' ] );
				},
				error_ => this.error = error_.message
			)
		;
	};
}
