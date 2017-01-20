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
		me.loading = true;
		this._scriptsService.getScripts()
			.subscribe(
				function( scripts_: Script[]) {
					me.loading = false;
					me.scripts = scripts_;
				},
				function( error_: String ) {
					me.loading = false;
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
