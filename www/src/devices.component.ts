import { Component, OnInit, AfterViewChecked } from '@angular/core';
import { Device, DeviceService } from './device.service';

declare var $: any;

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices.html',
	providers: [ DeviceService ]
} )

export class DevicesComponent implements OnInit, AfterViewChecked {

	private _devicesTable: any;

	error: string;
	devices: Device[];
	selectedDevice: Device;

	constructor( private _deviceService: DeviceService ) { };

	ngOnInit() {
		var me = this;

		me.getDevices();
	};

	ngAfterViewChecked(): void {
		this.resizeView();
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
		if ( this.selectedDevice == device_ ) {
			this.selectedDevice = null;
		} else {
			this.selectedDevice = device_
		}
	};

	submitDevice() {
		var me = this;
		this._deviceService.putDevice( me.selectedDevice )
			.subscribe(
				function( device_: Device[]) {
					alert( 'binnen' );
				},
				function( error_: string ) {
				}
			)
		;
	};

	resizeView() {
		var iWindowHeight = $(window).innerHeight();
		$('#resize_target').css( 'height', Math.max( 50, iWindowHeight - 130 ) );
	};

}
