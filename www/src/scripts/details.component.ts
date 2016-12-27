import { Component, OnInit }      from '@angular/core';
import { Router, ActivatedRoute } from '@angular/router';
import { Script, ScriptsService } from './scripts.service';

declare var $: any;
declare var ace: any;

@Component( {
	templateUrl: 'tpl/script-details.html',
} )

export class ScriptDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	script: Script;

	private _editor: any;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _scriptsService: ScriptsService
	) {
	};

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.script = data_.script;

			me._editor = ace.edit( 'script_editor_target' );
			me._editor.setTheme( 'ace/theme/crimson_editor' );
			me._editor.$blockScrolling = Infinity;
			me._editor.session.setMode( 'ace/mode/javascript' );
			me._editor.session.setUseSoftTabs( false );
			me._editor.setValue( me.script.code, -1 );
			me._editor.focus();
		} );
	};

	submitScript() {
		var me = this;
		me.loading = true;
		me.script.code = me._editor.getValue();
		this._scriptsService.putScript( me.script )
			.subscribe(
				function( script_: Script ) {
					me.loading = false;
					me._router.navigate( [ '/scripts' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	deleteScript() {
		var me = this;
		me._scriptsService.deleteScript( me.script )
			.subscribe(
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {
						me._router.navigate( [ '/scripts' ] );
					} else {
						this.error = 'Unable to delete script.';
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	cancelEdit() {
		this._router.navigate( [ '/scripts' ] );
	};

}
