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
	Device,
	DevicesService
}                         from './devices.service';
import { Hardware }       from '../hardware/hardware.service';
import { Script }         from '../scripts/scripts.service';
import { SessionService } from '../session/session.service';
import {
	GridPagingComponent
}                         from '../grid/paging.component'

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices-list.html',
	exportAs: 'listComponent' // can be used to communicate with parent component
} )

export class DevicesListComponent implements OnInit, OnDestroy {

	private _active: boolean = true;

	public loading: boolean = false;
	public error: String;
	public devices: Device[];
	public startPage: number = 1;
	
	@Input() public hardware?: Hardware; // gets set when used from the hardware edit component
	@Input() public parent?: Hardware;
	@Input() public script?: Script; // gets set when used from scripts
	@ViewChild(GridPagingComponent) private _paging: GridPagingComponent;

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data
			.subscribe( function( data_: any ) {
				me.devices = data_.devices;

				// TODO upon first load iterate all screens for each device and see if a screen has become obsolete due
				// to a device being removed. Remove the screen if so.

				if ( !!me.hardware ) {
					me.startPage = me._devicesService.lastPage[me.hardware.id] || 1;
				} else {
					me.startPage = me._devicesService.lastPage['global'] || 1;
				}
			} )
		;
		this._sessionService.events
			.takeWhile( () => this._active )
			.subscribe( function( event_: any ) {
				console.log( 'got an event in device list component' );
				console.log( event_ );
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
		if ( this._paging ) {
			if ( !!this.hardware ) {
				this._devicesService.lastPage[this.hardware.id] = this._paging.getActivePage();
			} else {
				this._devicesService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectDevice( device_: Device ) {
		this.loading = true;
		if ( this.hardware ) {
			if ( this.parent ) {
				this._router.navigate( [ '/hardware', this.parent.id, this.hardware.id, 'device', device_.id, 'edit' ] );
			} else {
				this._router.navigate( [ '/hardware', this.hardware.id, 'device', device_.id, 'edit' ] );
			}
		} else if ( this.script ) {
			this._router.navigate( [ '/scripts', this.script.id, 'device', device_.id, 'edit' ] );
		} else {
			this._router.navigate( [ '/devices', device_.id, 'edit' ] );
		}
	};
}
