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
import { Hardware }       from '../hardware/hardware.service';
import { Script }         from '../scripts/scripts.service';
import { SessionService } from '../session/session.service';
import {
	GridPagingComponent
}                         from '../grid/paging.component'

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices-list.html',
	exportAs: 'listComponent'
} )

export class DevicesListComponent implements OnInit, OnDestroy {

	private _active: boolean = true;

	public error: String;
	public devices: Device[];
	public startPage: number = 1;
	
	@Input() public hardware?: Hardware;
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
		var me = this;
		me._route.data
			.subscribe( function( data_: any ) {
				me.devices = data_.devices;
				if ( !! me.hardware ) {
					me.startPage = me._devicesService.lastPage['hardware_' + me.hardware.id] || 1;
				} else if ( !!me.script ) {
					me.startPage = me._devicesService.lastPage['script_' + me.script.id] || 1;
				} else {
					me.startPage = me._devicesService.lastPage['global'] || 1;
				}
			} )
		;
		me._sessionService.events
			.takeWhile( () => this._active )
			.subscribe( function( event_: any ) {
				let device: Device = me.devices.find( device_ => device_.id === event_.device_id );
				if ( !! device ) {
					device.value = event_.value;
					device.age = 0;
				}
			} )
		;

		Observable.interval( 1000 )
			.takeWhile( () => me._active )
			.subscribe( function() {
				for ( let i: number = 0; i < me.devices.length; i++ ) {
					me.devices[i].age += 1;
				}
			} )
		;
	};

	public ngOnDestroy() {
		this._active = false;
		if ( !! this._paging ) {
			if ( !! this.hardware ) {
				this._devicesService.lastPage['hardware_' + this.hardware.id] = this._paging.getActivePage();
			} else if ( !!this.script ) {
				this._devicesService.lastPage['script_' + this.script.id] = this._paging.getActivePage();
			} else {
				this._devicesService.lastPage['global'] = this._paging.getActivePage();
			}
		}
	};

	public selectDevice( device_: Device ) {
		this._devicesService.returnUrl = this._router.url;
		this._router.navigate( [ '/devices', device_.id, 'edit' ] );
	};

}
