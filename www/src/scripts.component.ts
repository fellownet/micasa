import { Component, OnInit } from '@angular/core';
import { Script, ScriptService } from './script.service';

declare var $: any;
declare var ace: any;

@Component( {
	selector: 'scripts',
	templateUrl: 'tpl/scripts.html',
	providers: [ ScriptService ]
} )

export class ScriptsComponent implements OnInit {

	private _scriptsTable: any;
	private _scriptEditor: any;

	error: string;
	scripts: Script[];
	selectedScript: Script;
	editorScript: Script;

	constructor( private _scriptService: ScriptService ) { };

	ngOnInit() {
		var me = this;

		me.getScripts();

		me._scriptsTable = $( '#scripts' ).dataTable( {
			// config here
		} );
		me._scriptsTable.on( 'click', 'tr', function() {
			if ( $( this ).hasClass( 'selected' ) ) {
				$( this ).removeClass( 'selected' );
				me.selectedScript = null;

			} else {
				var oData = me._scriptsTable.fnGetData( this );
				if ( oData && oData.script ) {
					me._scriptsTable.$( 'tr.selected' ).removeClass( 'selected' );
					$( this ).addClass( 'selected' );
					me.selectedScript = oData.script;
				}
			}
		} );

		this._scriptEditor = ace.edit( 'script_editor_target' );
		this._scriptEditor.setTheme( 'ace/theme/crimson_editor' );
		this._scriptEditor.session.setMode( 'ace/mode/javascript' );
		this._scriptEditor.session.setUseSoftTabs( false );
	};

	getScripts() {
		var me = this;
		this._scriptService.getScripts()
			.subscribe(
				function( scripts_: Script[]) {
					// TODO maybe try to find a way to do this the 'angular' way?
					me.scripts = scripts_;
					me._scriptsTable.fnClearTable();
					$.each( me.scripts, function( iIndex_: number, oScript_: Script ) {
						me._scriptsTable.fnAddData( { script: oScript_, '0':oScript_.id, '1':oScript_.name, '2':oScript_.status } );
					} );
				},
				error => this.error = <any>error
			)
		;
	};

	addScript() {
		this.editorScript = new Script();
		this._scriptEditor.setValue( '// enter code here' );
		this._scriptEditor.focus();
	};

	editScript() {
		this.editorScript = this.selectedScript;
		this._scriptEditor.setValue( this.editorScript.code, -1 );
		this._scriptEditor.focus();
	};

	deleteScript() {
		var me = this;
		me._scriptService.deleteScript( me.selectedScript )
			.subscribe(
				function( success_:boolean ) {
					if ( success_ ) {
						me.selectedScript = null;
						me.error = null;
					} else {
						me.error = 'Unable to delete script.';
					}
				},
				error => this.error = <any>error
			)
		;
	};

	cancelScript() {
		this.editorScript = null;
		// TODO replace this with a button on the error message > clearError or something
		this.error = null;
	};

	saveScript() {
		var me = this;
		me.editorScript.code = me._scriptEditor.getValue();
		me._scriptService.putScript( me.editorScript )
			.subscribe(
				function( success_:boolean ) {
					if ( success_ ) {
						me.editorScript = null;
						me.error = null;
					} else {
						me.error = 'Unable to save script.';
					}
				},
				error => this.error = <any>error
			)
		;
	};

}
