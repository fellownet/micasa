import { Component, OnInit, AfterViewChecked } from '@angular/core';
import { Script, ScriptService } from './script.service';

declare var $: any;
declare var ace: any;

@Component( {
	selector: 'scripts',
	templateUrl: 'tpl/scripts.html',
	providers: [ ScriptService ]
} )

export class ScriptsComponent implements OnInit, AfterViewChecked {

	private _scriptsTable: any;
	private _scriptEditor: any;

	error: string;
	scripts: Script[] = [];
	selectedScript: Script;
	editorScript: Script;

	constructor( private _scriptService: ScriptService ) { };

	ngOnInit() {
		var me = this;

		me.getScripts();

		this._scriptEditor = ace.edit( 'script_editor_target' );
		this._scriptEditor.setTheme( 'ace/theme/crimson_editor' );
		this._scriptEditor.$blockScrolling = Infinity;
		this._scriptEditor.session.setMode( 'ace/mode/javascript' );
		this._scriptEditor.session.setUseSoftTabs( false );
	};

	ngAfterViewChecked(): void {
		this.resizeView();
	};

	getScripts() {
		var me = this;
		this._scriptService.getScripts()
			.subscribe(
				function( scripts_: Script[]) {
					if ( scripts_ ) {
						me.scripts = scripts_;
					} else {
						me.error = 'Unable to get scripts.';
					}
				},
				error => this.error = <any>error
			)
		;
	};

	selectScript( script_: Script ) {
		if ( this.selectedScript == script_ ) {
			this.selectedScript = null;
		} else {
			this.selectedScript = script_
		}
	};

	addScript() {
		this.selectedScript = null;
		this.editorScript = { id: NaN, name: 'New script' };
		this._scriptEditor.setValue( '// enter code here' );
		this._scriptEditor.focus();
	};

	editScript() {
		this.editorScript = Object.assign( {}, this.selectedScript );
		this._scriptEditor.setValue( this.editorScript.code, -1 );
		this._scriptEditor.focus();
	};

	deleteScript( bConfirmed_: boolean ) {
		if ( ! bConfirmed_ ) {
			$('#delete-confirmation-dialog').modal();
		} else {
			var me = this;
			me._scriptService.deleteScript( me.selectedScript )
				.subscribe(
					function( success_:boolean ) {
						if ( success_ ) {
							var index = me.scripts.indexOf( me.selectedScript );
							me.scripts.splice( index, 1 );
							me.selectedScript = null;
							me.error = null;
						} else {
							me.error = 'Unable to delete script.';
						}
					},
					error => this.error = <any>error
				)
			;
		}
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
				function( script_: Script ) {
					if ( script_ ) {
						if ( me.selectedScript ) {
							Object.assign( me.selectedScript || {}, script_ )
						} else {
							me.scripts.push( script_ );
							me.selectedScript = script_;
						}
						me.editorScript = null;
						me.error = null;
					} else {
						me.error = 'Unable to save script.';
					}
				},
				error => me.error = <any>error
			)
		;
	};

	resizeView() {
		var iWindowHeight = $(window).innerHeight();
		$('#script_editor_target').css( 'height', Math.max( 50, iWindowHeight - 300 ) );
		$('#table_scripts').css( 'height', Math.max( 50, iWindowHeight - 164 ) );
	};

}
