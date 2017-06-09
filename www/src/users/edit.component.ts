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

	public error: String;
	public user: User;
	public title: string;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _usersService: UsersService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.user = data_.user;
			me.title = me.user.name;
		} );
	};

	public submitUser() {
		var me = this;
		this._usersService.putUser( me.user )
			.subscribe(
				function( user_: User ) {
					if ( !!me._usersService.returnUrl ) {
						me._router.navigateByUrl( me._usersService.returnUrl );
						delete me._usersService.returnUrl;
					} else {
						me._router.navigate( [ '/users' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	public deleteUser() {
		var me = this;
		me._usersService.deleteUser( me.user )
			.subscribe(
				function( user_: User ) {
					if ( !!me._usersService.returnUrl ) {
						me._router.navigateByUrl( me._usersService.returnUrl );
						delete me._usersService.returnUrl;
					} else {
						me._router.navigate( [ '/users' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};
}
