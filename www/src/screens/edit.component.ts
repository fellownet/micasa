import { Component, OnInit }         from '@angular/core';
import { Router, ActivatedRoute }    from '@angular/router';
import { Screen, ScreensService }    from './screens.service';

@Component( {
	templateUrl: 'tpl/screen-edit.html',
} )

export class ScreenEditComponent implements OnInit {

	loading: boolean = false;
	error: String;
	screen: Screen;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _screensService: ScreensService
	) {
	};

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.screen;
		} );
	};

	submitScreen() {
		var me = this;
		me.loading = true;
		this._screensService.putScreen( me.screen )
			.subscribe(
				function( screen_: Screen ) {
					me.loading = false;
					me._router.navigate( [ '/screen', screen_.index ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	deleteScreen() {
		var me = this;
		me.loading = true;
		me._screensService.deleteScreen( me.screen )
			.subscribe(
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {

						// TODO previous screen

						me._router.navigate( [ '/dashboard' ] );
					} else {
						this.error = 'Unable to delete screen.';
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
