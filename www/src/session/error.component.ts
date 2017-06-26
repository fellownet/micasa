import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import {
	Error
}                  from './session.service';

@Component( {
	templateUrl: 'tpl/error.html'
} )

export class ErrorComponent implements OnInit {

	public error: Error;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.error = data_.error;
				}
			)
		;
	};

}
