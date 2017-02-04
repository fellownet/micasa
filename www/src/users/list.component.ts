import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import { User }    from './users.service';

@Component( {
	templateUrl: 'tpl/users-list.html',
} )

export class UsersListComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public users: User[];

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.users = data_.users;
		} );
	};

	public selectUser( user_: User ) {
		this.loading = true;
		this._router.navigate( [ '/users', user_.id ] );
	};

	public addUser() {
		this.loading = true;
		this._router.navigate( [ '/users', 'add' ] );
	};
}
