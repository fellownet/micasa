import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import {
	Script,
	ScriptsService
}                  from './scripts.service';

import 'ace-builds/src-min-noconflict/ace.js';
import 'ace-builds/src-min-noconflict/theme-crimson_editor.js';
import 'ace-builds/src-min-noconflict/mode-javascript.js';

declare var ace: any;

@Component( {
	templateUrl: 'tpl/script-edit.html'
} )

export class ScriptEditComponent implements OnInit {

	public script: Script;

	public title: string;

	private _editor: any;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _scriptsService: ScriptsService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.script = data_.script;

					this.title = this.script.name;

					this._editor = ace.edit( 'script_editor_target' );
					this._editor.setTheme( 'ace/theme/crimson_editor' );
					this._editor.$blockScrolling = Infinity;
					this._editor.session.setUseSoftTabs( false );
					this._editor.session.setUseWorker( false );
					this._editor.session.setMode( 'ace/mode/javascript' );
					this._editor.session.setValue( this.script.code, -1 );
					this._editor.focus();
				}
			)
		;
	};

	public submitScript() {
		this.script.code = this._editor.getValue();
		this._scriptsService.putScript( this.script )
			.subscribe(
				script_ => {
					if ( !! this._scriptsService.returnUrl ) {
						this._router.navigateByUrl( this._scriptsService.returnUrl );
						delete this._scriptsService.returnUrl;
					} else {
						this._router.navigate( [ '/scripts' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deleteScript() {
		this._scriptsService.deleteScript( this.script )
			.subscribe(
				script_ => {
					if ( !! this._scriptsService.returnUrl ) {
						this._router.navigateByUrl( this._scriptsService.returnUrl );
						delete this._scriptsService.returnUrl;
					} else {
						this._router.navigate( [ '/scripts' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
