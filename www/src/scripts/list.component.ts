import { Component, OnInit }      from '@angular/core';
import { Router }                 from '@angular/router';
import { Script, ScriptsService } from './scripts.service';

declare var $: any;
declare var ace: any;

@Component( {
	templateUrl: 'tpl/scripts-list.html',
} )

export class ScriptsListComponent implements OnInit {

	loading: Boolean = false;
	error: String;
	scripts: Script[] = [];

	constructor(
		private _router: Router,
		private _scriptsService: ScriptsService
	) {
	};


	ngOnInit() {
		this.getScripts();
	};

	getScripts() {
		var me = this;
		this._scriptsService.getScripts()
			.subscribe(
				function( scripts_: Script[]) {
					me.scripts = scripts_;
				},
				function( error_: String ) {
					me.error = error_;
				}
			)
		;
	};

	selectScript( script_: Script ) {
		this._router.navigate( [ '/scripts', script_.id ] );
	};

	addScript() {
		this._router.navigate( [ '/scripts', 'add' ] );
	};

}
