import {
	Component,
	OnInit,
	OnDestroy,
	Input,
	ViewChild
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';

import {
	Plugin,
	PluginsService
}                         from './plugins.service';
import { SessionService } from '../session/session.service';
import {
	GridPagingComponent
}                         from '../grid/paging.component'

@Component( {
	selector: 'plugins',
	templateUrl: 'tpl/plugins-list.html',
} )

export class PluginsListComponent implements OnInit, OnDestroy {

	private _active: boolean = true;

	public plugins: Plugin[];
	public startPage: number = 1;

	@Input() public parent?: Plugin;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _pluginsService: PluginsService,
		private _sessionService: SessionService
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

		this._sessionService.events
			.takeWhile( () => this._active )
			.filter( event_ => event_.event == 'plugin_update' )
			.subscribe( event_ => {
				let plugin: Plugin = this.plugins.find( plugin_ => plugin_.id === event_.data.id );
				if ( !! plugin ) {
					plugin.state = event_.data.state;
					plugin.enabled = event_.data.enabled;
				}
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
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
