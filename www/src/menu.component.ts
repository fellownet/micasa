import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	ACL,
	UsersService
}                  from './users/users.service';
import {
	Screen,
	ScreensService
}                  from './screens/screens.service';

declare var $: any;

@Component( {
	selector: 'aside',
	templateUrl: 'tpl/menu.html',
} )

export class MenuComponent implements OnInit {

	public screens: Screen[] = [];

	// Make the ACL's available in the template.
	public ACL = ACL;

	public constructor(
		private _usersService: UsersService,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		this.getScreens();
	};

	public getScreens() {
		var me = this;
		this._screensService.getScreens()
			.subscribe(
				function( screens_: Screen[] ) {
					me.screens = screens_;
				},
				function( error_: String ) {
					me.screens = []; // not even the dashboard :(
				}
			)
		;
	};

	public hasAccess( acl_: ACL ): boolean {
		return this._usersService.isLoggedIn( acl_ );
	};

	public toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		}
	};

}
