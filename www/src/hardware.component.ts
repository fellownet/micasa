import { Component, OnInit } from '@angular/core';
import { Hardware, HardwareService } from './hardware.service';

declare var $: any;

@Component( {
	selector: 'hardware',
	templateUrl: 'tpl/hardware.html',
	providers: [ HardwareService ]
} )

export class HardwareComponent implements OnInit {

	private _hardwareTable: any;

	error: string;
	hardware: Hardware[];
	selectedHardware: Hardware;

	openzwaveMode: string;
	openzwaveHardware: Hardware;

	constructor( private _hardwareService: HardwareService ) { };

	ngOnInit() {
		var me = this;

		me.getHardware();
	};

	getHardware() {
		var me = this;
		this._hardwareService.getHardware()
			.subscribe(
				function( hardware_: Hardware[]) {
					me.hardware = hardware_;
				},
				error => this.error = <any>error
			)
		;
	};

	selectHardware( hardware_: Hardware ) {
		if ( this.selectedHardware == hardware_ ) {
			this.selectedHardware = null;
		} else {
			this.selectedHardware = hardware_
		}
	};

	submitHardware() {
		var me = this;
		this._hardwareService.putHardware( me.selectedHardware )
			.subscribe(
				function( hardware_: Hardware[]) {

					alert( 'binnen' );
				},
				function( error_: string ) {
				}
			)
		;
	};

	/* methods specifically for openzwave */

	openzwaveIncludeMode( hardware_: Hardware ) {
		var me = this;
		me.openzwaveMode = 'include';
		me.openzwaveHardware = hardware_;
		me._hardwareService.openzwaveIncludeMode( hardware_ )
			.subscribe(
				function() {
					$('#openzwave-action').modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	openzwaveExcludeMode( hardware_: Hardware ) {
		var me = this;
		me.openzwaveMode = 'exclude';
		me.openzwaveHardware = hardware_;
		me._hardwareService.openzwaveExcludeMode( hardware_ )
			.subscribe(
				function() {
					$('#openzwave-action').modal();
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

	exitMode() {
		var me = this;
		if ( me.openzwaveMode == 'include' ) {
			me._hardwareService.exitOpenzwaveIncludeMode( me.openzwaveHardware )
				.subscribe(
					function() {
						me.openzwaveMode = null;
						me.openzwaveHardware = null;
						me.getHardware();
					},
					function( error_: string ) {
						me.error = error_;
					}
				)
			;
		} else {
			me._hardwareService.exitOpenzwaveExcludeMode( me.openzwaveHardware )
				.subscribe(
					function() {
						me.openzwaveMode = null;
						me.openzwaveHardware = null;
						me.getHardware();
					},
					function( error_: string ) {
						me.error = error_;
					}
				)
			;
		}
	};

}
