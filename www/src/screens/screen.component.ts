import {
	Component,
	OnInit
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';

import {
	SourceData,
	Screen
}                         from './screens.service';

@Component( {
	templateUrl: 'tpl/screen.html'
} )

export class ScreenComponent implements OnInit {

	public screen: Screen;
	public data: SourceData[][];

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.screen = data_.payload.screen;
					this.data = data_.payload.data;
				}
			)
		;
	};

}
