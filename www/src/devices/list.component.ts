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
import { Observable }     from 'rxjs/Observable';

import {
	Device,
	DevicesService
}                         from './devices.service';
import { Plugin }         from '../plugins/plugins.service';
import { Script }         from '../scripts/scripts.service';
import { SessionService } from '../session/session.service';
import {
	GridPagingComponent
}                         from '../grid/paging.component'

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices-list.html'
} )

export class DevicesListComponent implements OnInit, OnDestroy {

	private _active: boolean = true;

	public devices: Device[];
	public startPage: number = 1;

	@Input() public plugin?: Plugin;
	@Input() public script?: Script;

	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		this._route.data
			.subscribe(
				data_ => {
					this.devices = data_.devices;

					if ( !! this.plugin ) {
						this.startPage = this._devicesService.lastPage['plugin_' + this.plugin.id] || 1;
					} else if ( !! this.script ) {
						this.startPage = this._devicesService.lastPage['script_' + this.script.id] || 1;
					} else {
						this.startPage = this._devicesService.lastPage['global'] || 1;
					}
				}
			)
		;

		this._sessionService.events
			.takeWhile( () => this._active )
			.filter( event_ => event_.event == 'device_update' )
			.filter( event_ => ! this.plugin || this.plugin.id == event_.data.plugin_id )
			.subscribe( event_ => {
				let device: Device = this.devices.find( device_ => device_.id === event_.data.id );
				if ( !! device ) {
					device.value = event_.data.value;
					device.age = 0;
				}
			} )
		;

		Observable.interval( 1000 )
			.takeWhile( () => this._active )
			.subscribe( () => {
				for ( let i: number = 0; i < this.devices.length; i++ ) {
					this.devices[i].age += 1;
					if ( this.devices[i].next_schedule > 0 ) {
						this.devices[i].next_schedule -= 1;
					} else {
						this.devices[i].scheduled = false;
					}
				}
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
		if ( !! this._paging ) {
			if ( !! this.plugin ) {
				this._devicesService.lastPage['plugin_' + this.plugin.id] = this._paging.getActivePage();
			} else if ( !!this.script ) {
				this._devicesService.lastPage['script_' + this.script.id] = this._paging.getActivePage();
			} else {
				this._devicesService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectDevice( device_: Device ) {
		this._devicesService.returnUrl = this._router.url;
		this._router.navigate( [ '/devices', device_.id ] );
	};

}
