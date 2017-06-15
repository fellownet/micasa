import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import {
	User,
	UsersService
}                  from './users.service';

@Component( {
	templateUrl: 'tpl/user-edit.html',
} )

export class UserEditComponent implements OnInit {

	public user: User;
	public title: string;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _usersService: UsersService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.user = data_.user;

					this.title = this.user.name;
				}
			)
		;
	};

	public submitUser() {
		this._usersService.putUser( this.user )
			.subscribe(
				user_ => {
					if ( !! this._usersService.returnUrl ) {
						this._router.navigateByUrl( this._usersService.returnUrl );
						delete this._usersService.returnUrl;
					} else {
						this._router.navigate( [ '/users' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteUser() {
		this._usersService.deleteUser( this.user )
			.subscribe(
				user_ => {
					if ( !! this._usersService.returnUrl ) {
						this._router.navigateByUrl( this._usersService.returnUrl );
						delete this._usersService.returnUrl;
					} else {
						this._router.navigate( [ '/users' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
