import { Component, OnInit } from '@angular/core';
import { Hardware, HardwareService } from './hardware.service';

declare var $: any;

// TODO https://angular.io/docs/ts/latest/guide/forms.html
// the document uses npm to add bootstrap, why do we do this manually?

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

	constructor( private _hardwareService: HardwareService ) { };

	ngOnInit() {
		var me = this;

		me.getHardware();

		me._hardwareTable = $( '#hardware' ).dataTable( {
			// config here
		} );
		me._hardwareTable.on( 'click', 'tr', function() {
			if ( $( this ).hasClass( 'selected' ) ) {
				$( this ).removeClass( 'selected' );
				me.selectedHardware = null;

			} else {
				var oData = me._hardwareTable.fnGetData( this );
				if ( oData && oData.hardware ) {
					me._hardwareTable.$( 'tr.selected' ).removeClass( 'selected' );
					$( this ).addClass( 'selected' );
					me.selectedHardware = oData.hardware;
				}
			}
		} );
	};

	getHardware() {
		var me = this;
		this._hardwareService.getHardware()
			.subscribe(
				function( hardware_: Hardware[]) {
					// TODO maybe try to find a way to do this the 'angular' way?
					me.hardware = hardware_;
					me._hardwareTable.fnClearTable();
					$.each( me.hardware, function( iIndex_: number, oHardware_: Hardware ) {
						me._hardwareTable.fnAddData( { hardware: oHardware_, '0':oHardware_.id, '1':oHardware_.name, '2':oHardware_.label } );
					} );
				},
				error => this.error = <any>error
			)
		;
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

}
