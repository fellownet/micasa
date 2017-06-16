// https://github.com/angular/angular/issues/10251#issuecomment-238709839

import {
	Directive,
	Input,
	OnInit,
}                 from '@angular/core';

import {
	NgModel,
	NgForm
}                 from '@angular/forms';

@Directive( {
	selector: '[registerForm]'
} )

export class RegisterFormModelDirective implements OnInit {

	@Input( "registerForm" ) private _form: NgForm;
	@Input( "registerModel" ) private _model: NgModel;

	public ngOnInit() {
		if (
			!! this._form
			&& !! this._model
		) {
			this._form.form.registerControl( this._model.name, this._model.control );
			this._form.addControl( this._model );
		}
	};

}
