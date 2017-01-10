import { Component, OnInit }         from '@angular/core';
import { Router, ActivatedRoute }    from '@angular/router';
import {
	Hardware, Setting, Option,
	HardwareService
}                                    from './hardware.service';
import { Device }                    from '../devices/devices.service';

declare var $: any;

@Component( {
	templateUrl: 'tpl/hardware-details.html',
} )

export class HardwareDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	hardware: Hardware;

	hasAdvancedSettings: boolean;
	showAdvancedSettings: boolean;

	zwavemode: string;

	constructor(
		private _router: Router,
		private _route: ActivatedRoute,
		private _hardwareService: HardwareService
	) {
	};

	ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			me.hardware = data_.hardware;
			for ( let setting of me.hardware.settings ) {
				if ( setting.class == 'advanced' ) {
					me.hasAdvancedSettings = true;
				}
			}
		} );
	};

	submitHardware() {
		var me = this;
		me.loading = true;
		this._hardwareService.putHardware( me.hardware )
			.subscribe(
				function( hardware_: Hardware ) {
					me.loading = false;
					me._router.navigate( [ '/hardware' ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
					window.scrollTo( 0, 0 );
				}
			)
		;
	};

	deleteHardware() {
		var me = this;
		me.loading = true;
		me._hardwareService.deleteHardware( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.loading = false;
					if ( success_ ) {
						me._router.navigate( [ '/hardware' ] );
					} else {
						this.error = 'Unable to delete hardware.';
					}
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	// Methods specifically for the Z-Wave hardware.

	zwaveIncludeMode() {
		var me = this;
		this._hardwareService.zwaveIncludeMode( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.zwavemode = 'include';
					$( '#zwave-action' ).modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	zwaveExcludeMode() {
		var me = this;
		this._hardwareService.zwaveExcludeMode( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.zwavemode = 'exclude';
					$( '#zwave-action' ).modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	zwaveExitMode() {
		var me = this;
		if ( 'include' == me.zwavemode ) {
			this._hardwareService.exitZWaveIncludeMode( me.hardware )
				.subscribe(
					function( success_: boolean ) {
						me.zwavemode = null;
					},
					function( error_: string ) {
						me.error = error_;
					}
				)
			;
		} else if ( 'exclude' == me.zwavemode ) {
			this._hardwareService.exitZWaveExcludeMode( me.hardware )
				.subscribe(
					function( success_: boolean ) {
						me.zwavemode = null;
					},
					function( error_: string ) {
						me.error = error_;
					}
				)
			;
		}
	};

	zwaveHealNetwork() {
		var me = this;
		this._hardwareService.zwaveHealNetwork( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.zwavemode = 'heal';
					$( '#zwave-action' ).modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	zwaveHealNode() {
		var me = this;
		this._hardwareService.zwaveHealNode( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.zwavemode = 'heal';
					$( '#zwave-action' ).modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	// Methods for the Dummy Hardware hardware.

	dummyAddDevice( type_: string ) {
		var me = this;
		me.loading = true;
		this._hardwareService.dummyAddDevice( me.hardware, type_ )
			.subscribe(
				function( device_: Device ) {
					me.loading = false;
					me._router.navigate( [ '/hardware/' + me.hardware.id + '/' + device_.id ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

}
