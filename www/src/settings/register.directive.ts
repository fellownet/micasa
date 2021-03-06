// https://github.com/angular/angular/issues/10251#issuecomment-238709839

import {
	Directive,
	Input,
	OnInit,
	OnDestroy
}                 from '@angular/core';

import {
	NgModel,
	NgForm
}                 from '@angular/forms';

@Directive( {
	selector: '[registerForm]'
} )

export class RegisterFormModelDirective implements OnInit, OnDestroy {

	@Input( "registerForm" ) private _form: NgForm;
	@Input( "registerModel" ) private _model: NgModel;

	public ngOnInit() {
		if (
			!! this._form
			&& !! this._model
		) {
			this._form.addControl( this._model );
		}
	};

	public ngOnDestroy() {
		if (
			!! this._form
			&& !! this._model
		) {
			this._form.removeControl( this._model );
		}

	};

}
