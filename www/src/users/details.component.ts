import { Component, OnInit }      from '@angular/core';
import { Router, ActivatedRoute } from '@angular/router';
import { User, UsersService }     from './users.service';

declare var $: any;
declare var ace: any;

@Component( {
	templateUrl: 'tpl/user-details.html',
} )

export class UserDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	user: User;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _usersService: UsersService
	) {
	};

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.user = data_.user;
		} );
	};

	submitUser() {
		var me = this;
		me.loading = true;
		this._usersService.putUser( me.user )
			.subscribe(
				function( user_: User ) {
					me.loading = false;
					me._router.navigate( [ '/users' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	deleteUser() {
		var me = this;
		me.loading = true;
		me._usersService.deleteUser( me.user )
			.subscribe(
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {
						me._router.navigate( [ '/users' ] );
					} else {
						this.error = 'Unable to delete user.';
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

}
