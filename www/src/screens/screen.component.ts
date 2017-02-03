import {
	Component,
	OnInit,
	OnDestroy
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';

import { SessionService } from '../session/session.service';

@Component( {
	templateUrl: 'tpl/screen.html',
} )

export class ScreenComponent implements OnInit, OnDestroy {

	private _active: boolean = true;

	public loading: boolean = false;
	public error: string;
	public screen: Screen;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.screen;
		} );
		this._sessionService.events
			.takeWhile( () => this._active )
			.subscribe( function( event_: any ) {
				console.log( 'got an event in screen component' );
				console.log( event_ );
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
	};

}
