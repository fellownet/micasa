import {
	Component,
	OnInit,
	OnDestroy,
	ViewChild
}                  from '@angular/core';
import {
	Router,
	ActivatedRoute
}                  from '@angular/router';
import {
	Script,
	ScriptsService
}                  from './scripts.service';
import {
	GridPagingComponent
}                  from '../grid/paging.component'

@Component( {
	templateUrl: 'tpl/scripts-list.html',
} )

export class ScriptsListComponent implements OnInit, OnDestroy {

	public error: String;
	public scripts: Script[];
	public startPage: number = 1;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _scriptsService: ScriptsService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.scripts = data_.scripts;
			me.startPage = me._scriptsService.lastPage || 1;
		} );
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			this._scriptsService.lastPage = this._paging.getActivePage();
		}
	};

	public selectScript( script_: Script ) {
		this._scriptsService.returnUrl = this._router.url;
		this._router.navigate( [ '/scripts', script_.id ] );
	};

	public addScript() {
		this._scriptsService.returnUrl = this._router.url;
		this._router.navigate( [ '/scripts', 'add' ] );
	};

}
