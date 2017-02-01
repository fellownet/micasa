import {
	Component,
	OnInit
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';

import { Script }  from './scripts.service';

@Component( {
	templateUrl: 'tpl/scripts-list.html',
} )

export class ScriptsListComponent implements OnInit {

	public loading: boolean = false;
	public error: String;
	public scripts: Script[];

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.scripts = data_.scripts;
		} );
	};

	public selectScript( script_: Script ) {
		this.loading = true;
		this._router.navigate( [ '/scripts', script_.id ] );
	};

	public addScript() {
		this.loading = true;
		this._router.navigate( [ '/scripts', 'add' ] );
	};

}
