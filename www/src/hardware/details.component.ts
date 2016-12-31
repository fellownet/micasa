import { Component, OnInit }         from '@angular/core';
import { Router, ActivatedRoute }    from '@angular/router';
import { Hardware, HardwareService } from './hardware.service';
import { Device }                    from '../devices/devices.service';

declare var $: any;

@Component( {
	templateUrl: 'tpl/hardware-details.html',
} )

export class HardwareDetailsComponent implements OnInit {

	loading: boolean = false;
	error: String;
	hardware: Hardware;

	openzwavemode: string;

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

	// Methods specifically for the OpenZWave hardware.

	openzwaveIncludeMode() {
		var me = this;
		this._hardwareService.openzwaveIncludeMode( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.openzwavemode = 'include';
					$( '#openzwave-action' ).modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	openzwaveExcludeMode() {
		var me = this;
		this._hardwareService.openzwaveExcludeMode( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.openzwavemode = 'exclude';
					$( '#openzwave-action' ).modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	openzwaveExitMode() {
		var me = this;
		if ( 'include' == me.openzwavemode ) {
			this._hardwareService.exitOpenzwaveIncludeMode( me.hardware )
				.subscribe(
					function( success_: boolean ) {
						me.openzwavemode = null;
					},
					function( error_: string ) {
						me.error = error_;
					}
				)
			;
		} else if ( 'exclude' == me.openzwavemode ) {
			this._hardwareService.exitOpenzwaveExcludeMode( me.hardware )
				.subscribe(
					function( success_: boolean ) {
						me.openzwavemode = null;
					},
					function( error_: string ) {
						me.error = error_;
					}
				)
			;
		}
	};

	openzwaveHealNetwork() {
		var me = this;
		this._hardwareService.openzwaveHealNetwork( me.hardware )
			.subscribe(
				function( success_: boolean ) {
					me.openzwavemode = 'heal';
					$( '#openzwave-action' ).modal();
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
