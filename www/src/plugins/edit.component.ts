import {
	Component,
	OnInit,
	ViewChild,
	Renderer2
}                               from '@angular/core';
import {
	Router,
	ActivatedRoute,
	NavigationEnd
}                               from '@angular/router';

import {
	Plugin,
	PluginsService
}                               from './plugins.service';
import {
	Device, DevicesService
}                               from '../devices/devices.service';

@Component( {
	templateUrl: 'tpl/plugin-edit.html'
} )

export class PluginEditComponent implements OnInit {

	public plugin: Plugin;
	public devices: Device[];

	public title: string;

	public hasAdvancedSettings: boolean = false;
	public hasActionDevices: boolean = false;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _renderer: Renderer2,
		private _pluginsService: PluginsService,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.plugin = data_.plugin;
					this.devices = data_.devices;

					this.title = this.plugin.name || 'New Plugin';

					if ( !! this.devices ) {
						for ( let device of this.devices ) {
							if ( device.subtype == 'action' ) {
								this.hasActionDevices = true;
								break;
							}
						}
					}
					for ( let setting of this.plugin.settings ) {
						if ( setting.class == 'advanced' ) {
							this.hasAdvancedSettings = true;
							break;
						}
					}
				}
			)
		;
		this._router.events
			.filter( event_ => event_ instanceof NavigationEnd )
			.subscribe( () => this._renderer.setProperty( document.body, 'scrollTop', 0 ) )
		;
	};

	public submitPlugin() {
		this._pluginsService.putPlugin( this.plugin )
			.subscribe(
				plugin_ => {
					if ( !! this._pluginsService.returnUrl ) {
						this._router.navigateByUrl( this._pluginsService.returnUrl );
						delete this._pluginsService.returnUrl
					} else {
						this._router.navigate( [ '/plugins' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public deletePlugin() {
		this._pluginsService.deletePlugin( this.plugin )
			.subscribe(
				plugin_ => {
					if ( !! this._pluginsService.returnUrl ) {
						this._router.navigateByUrl( this._pluginsService.returnUrl );
						delete this._pluginsService.returnUrl
					} else {
						this._router.navigate( [ '/plugins' ] );
					}
				},
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

	public performAction( action_: Device ) {
		this._devicesService.performAction( action_ )
			.subscribe(
				() => undefined,
				error_ => this._router.navigate( [ '/error' ] )
			)
		;
	};

}
