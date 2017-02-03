import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import {
	Screen,
	ScreensService
}                  from './screens.service';

@Component( {
	templateUrl: 'tpl/screen-edit.html',
} )

export class ScreenEditComponent implements OnInit {

	public loading: boolean = false;
	public error: string;
	public screen: Screen;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.screen;
		} );
	};

	public submitScreen() {
		var me = this;
		me.loading = true;
		this._screensService.putScreen( me.screen )
			.subscribe(
				function( screens_: Screen[] ) {
					me._router.navigate( [ '/screens', me.screen.id ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	public deleteScreen() {
		var me = this;
		me.loading = true;
		me._screensService.deleteScreen( me.screen )
			.subscribe(
				function( screens_: Screen[] ) {
					me._router.navigate( [ '/dashboard' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

}
