import { Component, OnInit }      from '@angular/core';
import { Router, ActivatedRoute } from '@angular/router';
import { Device, DeviceService }  from './device.service';

declare var $: any;

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices.html',
	providers: [ DeviceService ]
} )


// TODO split up and put in folder and name the list ListComponent and the details DetailsComponent
// and provide a DeviceModule or something. Details are in the routing examples.

export class DevicesComponent implements OnInit {

	private _devicesTable: any;

	error: string;
	loading: boolean = false;
	devices: Device[];
	selectedDevice: Device;

	constructor( private _router: Router, private _route: ActivatedRoute, private _deviceService: DeviceService ) { };

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			if ( data_.device ) {
				me.selectedDevice = data_.device;
			} else {
				me.selectedDevice = null;
			}
		} );
		me.getDevices();
	};

	getDevices() {
		var me = this;
		this._deviceService.getDevices()
			.subscribe(
				function( devices_: Device[]) {
					me.devices = devices_;
				},
				error => this.error = <any>error
			)
		;
	};

	selectDevice( device_: Device ) {
		this._router.navigate( [ '/devices', device_.id ] );
	};

	submitDevice() {
		var me = this;
		me.loading = true;
		this._deviceService.putDevice( me.selectedDevice )
			.subscribe(
				function( device_: Device[]) {
					me._router.navigate( [ '/devices' ] );
					me.loading = false;
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	cancelEdit() {
		this._router.navigate( [ '/devices' ] );
	};

}
