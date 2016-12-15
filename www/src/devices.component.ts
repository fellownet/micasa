import { Component, OnInit } from '@angular/core';
import { Device, DeviceService } from './device.service';

declare var $: any;

@Component( {
	selector: 'devices',
	templateUrl: 'tpl/devices.html',
	providers: [ DeviceService ]
} )

export class DevicesComponent implements OnInit {

	private _devicesTable: any;

	error: string;
	devices: Device[];
	selectedDevice: Device;

	constructor( private _deviceService: DeviceService ) { };

	ngOnInit() {
		var me = this;

		me.getDevices();

		me._devicesTable = $( '#devices' ).dataTable( {
			// config here
		} );
		me._devicesTable.on( 'click', 'tr', function() {
			if ( $( this ).hasClass( 'selected' ) ) {
				$( this ).removeClass( 'selected' );
				me.selectedDevice = null;

			} else {
				var oData = me._devicesTable.fnGetData( this );
				if ( oData && oData.device ) {
					me._devicesTable.$( 'tr.selected' ).removeClass( 'selected' );
					$( this ).addClass( 'selected' );
					me.selectedDevice = oData.device;
				}
			}
		} );
	};

	getDevices() {
		var me = this;
		this._deviceService.getDevices()
			.subscribe(
				function( devices_: Device[]) {
					// TODO maybe try to find a way to do this the 'angular' way?
					me.devices = devices_;
					me._devicesTable.fnClearTable();
					$.each( me.devices, function( iIndex_: number, oDevice: Device ) {
						me._devicesTable.fnAddData( { device: oDevice, '0':oDevice.id, '1':oDevice.hardware.name, '2':oDevice.type, '3':oDevice.name, '4':oDevice.label, '5':oDevice.value } );
					} );
				},
				error => this.error = <any>error
			)
		;
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

}
