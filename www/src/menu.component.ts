import { Component }  from '@angular/core';
import { Observable } from 'rxjs/Observable';

import { ACL }        from './users/users.service';
import {
	Session,
	SessionService
}                     from './session/session.service';
import {
	Screen,
	ScreensService
}                     from './screens/screens.service';

declare var $: any;

@Component( {
	selector: 'aside',
	templateUrl: 'tpl/menu.html',
} )

export class MenuComponent {

	public session: Observable<Session>;
	public screens: Observable<Screen[]>;
	public ACL = ACL;

	public constructor(
		private _sessionService: SessionService,
		private _screensService: ScreensService
	) {
		// Make the session- and screen observables from their respective services available
		// in the template.
		this.session = _sessionService.session;
		this.screens = _screensService.screens;
	};

	public toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		}
	};
}
