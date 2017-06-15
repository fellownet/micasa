import {
	Component,
	OnInit,
	ViewEncapsulation
}                         from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import {
	Session,
	SessionService
}                         from './session/session.service';
import {
	ScreensService,
	Screen
}                         from './screens/screens.service';
import { ACL }            from './users/users.service';

declare var $: any;
declare var navigator: any;

declare var require: any;
const Highcharts = require( 'highcharts/highcharts.src.js' );

@Component( {
	selector: 'body',
	templateUrl: 'tpl/app.html',
	encapsulation: ViewEncapsulation.None
} )

export class AppComponent implements OnInit {

	public session: Observable<Session>;
	public ACL = ACL;
	public screens: Observable<Screen[]>;

	public constructor(
		private _sessionService: SessionService,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		this.session = this._sessionService.session;

		// NOTE the getScreens method returns an observable that, once being subscribed to by the async pipe
		// from the template, returns the list of screens as being cached by the session service. Alterations
		// on the list of screens will end up in this cache too, and the ui will update itself accordingly.
		//this.screens = this._screensService.getScreens();
		this.screens = this.session
			.filter( session_ => !! session_ )
			.mergeMap( () => this._screensService.getScreens() )
		;
	};

	public get loading(): Observable<boolean> {
		return this._sessionService.loading;
	};

	public toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		} else {
			$( 'body' ).addClass( 'sidebar-open' );
		}
	};

	public hideMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		}
	};

}
