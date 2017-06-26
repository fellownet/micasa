import {
	Component,
	OnInit,
	OnDestroy,
	Input,
	ViewChild
}                   from '@angular/core';
import {
	Router,
	ActivatedRoute
}                   from '@angular/router';

import {
	Plugin,
	PluginsService
}                   from './plugins.service';
import {
	GridPagingComponent
}                   from '../grid/paging.component'

@Component( {
	selector: 'plugins',
	templateUrl: 'tpl/plugins-list.html',
} )

export class PluginsListComponent implements OnInit, OnDestroy {

	public plugins: Plugin[];
	public startPage: number = 1;

	@Input() public parent?: Plugin;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _pluginsService: PluginsService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					if ( 'plugins' in data_ ) {
						this.plugins = data_.plugins;
					} else if (
						'plugin' in data_
						&& !! data_.plugin.children
					) {
						this.plugins = data_.plugin.children;
					}

					if ( !! this.parent ) {
						this.startPage = this._pluginsService.lastPage[this.parent.id] || 1;
					} else {
						this.startPage = this._pluginsService.lastPage['global'] || 1;
					}
				}
			)
		;
	};

	public ngOnDestroy() {
		if ( this._paging ) {
			if ( !! this.parent ) {
				this._pluginsService.lastPage[this.parent.id] = this._paging.getActivePage();
			} else {
				this._pluginsService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectPlugin( plugin_: Plugin ) {
		this._pluginsService.returnUrl = this._router.url;
		this._router.navigate( [ '/plugins', plugin_.id ] );
	};

	public addPlugin() {
		this._pluginsService.returnUrl = this._router.url;
		this._router.navigate( [ '/plugins', 'add' ] );
	};

}
