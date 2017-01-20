import { Component, OnInit }      from '@angular/core';
import { Router, ActivatedRoute } from '@angular/router';
import { UsersService }           from '../users/users.service';

@Component( {
	templateUrl: 'tpl/screen.html',
} )

export class ScreenComponent implements OnInit {

	loading: Boolean = false;
	error: String;
	screen: Screen;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _usersService: UsersService
	) {
	};

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.screen;
		} );
	};

}
