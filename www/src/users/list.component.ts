import {
	Component,
	OnInit,
	OnDestroy,
	ViewChild
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import {
	User,
	UsersService
}                  from './users.service';
import {
	GridPagingComponent
}                  from '../grid/paging.component'

@Component( {
	templateUrl: 'tpl/users-list.html',
} )

export class UsersListComponent implements OnInit, OnDestroy {

	public users: User[];
	public startPage: number = 1;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

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
					this.users = data_.users;

					this.startPage = this._usersService.lastPage || 1;
				}
			)
		;
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			this._usersService.lastPage = this._paging.getActivePage();
		}
	};

	public selectUser( user_: User ) {
		this._usersService.returnUrl = this._router.url;
		this._router.navigate( [ '/users', user_.id ] );
	};

	public addUser() {
		this._usersService.returnUrl = this._router.url;
		this._router.navigate( [ '/users', 'add' ] );
	};

}
