import {
	Component,
	OnInit,
	ViewChild
}                  from '@angular/core';
import { NgForm }  from '@angular/forms';
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

	public error?: string;

	public screen: Screen;
	public title: string;

	@ViewChild( 'screenForm' ) private _form: NgForm;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.screen = data_.payload.screen;
			me.title = me.screen.name;
		} );
	};

	public submitScreen() {
		this._screensService.putScreen( this.screen )
			.subscribe(
				() => this._router.navigate( [ '/screens', this.screen.id ] ),
				error_ => this.error = error_
			)
		;
	};

	public deleteScreen() {
		this._screensService.deleteScreen( this.screen )
			.subscribe(
				() => this._router.navigate( [ '/dashboard' ] ),
				error_ => this.error = error_
			)
		;
	};

}
