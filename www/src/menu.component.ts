import { Component }             from '@angular/core';
import { ACL, UsersService }     from './users/users.service';
import { Screen, ScreenService } from './screen.service';

// Declaring $ as any prevents typescript errors when using jQuery.
declare var $: any;

@Component( {
	selector    : 'aside',
	templateUrl : 'tpl/menu.html',
} )

export class MenuComponent  {

	// Make the ACL's available in the template.
	public ACL = ACL;

	constructor( private _usersService: UsersService, private _screenService: ScreenService ) {
	}

	hasAccess( acl_: ACL ): boolean {
		return this._usersService.isLoggedIn( acl_ );
	}

	getScreens(): Screen[] {
		return this._screenService.getScreens();
	}

	toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		} else {
			$( 'body' ).addClass( 'sidebar-open' );
		}
	}

}
