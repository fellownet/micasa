import { Component, OnInit }  from '@angular/core';
import { Router }             from '@angular/router';
import { User, UsersService } from './users.service';

@Component( {
	templateUrl: 'tpl/users-list.html',
} )

export class UsersListComponent implements OnInit {

	loading: Boolean = false;
	error: String;
	users: User[] = [];

	constructor(
		private _router: Router,
		private _usersService: UsersService
	) {
	};


	ngOnInit() {
		this.getUsers();
	};

	getUsers() {
		var me = this;
		me.loading = true;
		this._usersService.getUsers()
			.subscribe(
				function( users_: User[]) {
					me.loading = false;
					me.users = users_;
				},
				function( error_: String ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	selectUser( user_: User ) {
		this._router.navigate( [ '/users', user_.id ] );
	};

	addUser() {
		this._router.navigate( [ '/users', 'add' ] );
	};

}
