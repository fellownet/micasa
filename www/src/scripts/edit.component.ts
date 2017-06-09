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

	public error: String;
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
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.script = data_.script;
			me.title = me.script.name;

			me._editor = ace.edit( 'script_editor_target' );
			me._editor.setTheme( 'ace/theme/crimson_editor' );
			me._editor.$blockScrolling = Infinity;
			me._editor.session.setUseSoftTabs( false );
			me._editor.session.setUseWorker( false );
			me._editor.session.setMode( 'ace/mode/javascript' );
			me._editor.session.setValue( me.script.code, -1 );
			me._editor.focus();
		} );
	};

	public submitScript() {
		var me = this;
		me.script.code = me._editor.getValue();
		this._scriptsService.putScript( me.script )
			.subscribe(
				function( script_: Script ) {
					if ( !!me._scriptsService.returnUrl ) {
						me._router.navigateByUrl( me._scriptsService.returnUrl );
						delete me._scriptsService.returnUrl;
					} else {
						me._router.navigate( [ '/scripts' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	public deleteScript() {
		var me = this;
		me._scriptsService.deleteScript( me.script )
			.subscribe(
				function( script_: Script ) {
					if ( !!me._scriptsService.returnUrl ) {
						me._router.navigateByUrl( me._scriptsService.returnUrl );
						delete me._scriptsService.returnUrl;
					} else {
						me._router.navigate( [ '/scripts' ] );
					}
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

}
