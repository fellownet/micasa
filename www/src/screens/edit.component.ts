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

	public screen: Screen;

	public title: string;

	public showDeleteWarning: boolean = false;

	@ViewChild( 'screenForm' ) private _form: NgForm;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.screen = data_.payload.screen;

					this.title = this.screen.name;

					this.showDeleteWarning = false;
				}
			)
		;
	};

	public submitScreen() {
		this._screensService.putScreen( this.screen )
			.subscribe(
				() => this._router.navigate( [ '/screens', this.screen.id ] ),
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteScreen() {
		this._screensService.deleteScreen( this.screen )
			.subscribe(
				() => this._router.navigate( [ '/dashboard' ] ),
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
