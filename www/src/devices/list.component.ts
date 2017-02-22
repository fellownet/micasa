import {
	Component,
	OnInit,
	OnDestroy,
	Input
}                         from '@angular/core';
import {
	Router,
	ActivatedRoute
}                         from '@angular/router';

import { Device }         from './devices.service';
import { Hardware }       from '../hardware/hardware.service';
import { Script }         from '../scripts/scripts.service';
import { SessionService } from '../session/session.service';

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

	@Input() public hardware?: Hardware; // gets set when used from the hardware edit component
	@Input() public parent?: Hardware;
	@Input() public script?: Script; // gets set when used from scripts

	public constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data
			.subscribe( function( data_: any ) {
				me.devices = data_.devices;
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
