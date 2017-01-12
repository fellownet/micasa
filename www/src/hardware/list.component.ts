import { Component, OnInit }               from '@angular/core';
import { Input, OnChanges, SimpleChanges } from '@angular/core';
import { Router }                          from '@angular/router';
import { Hardware, HardwareService }       from './hardware.service';

declare var $: any;

@Component( {
	selector: 'hardware',
	templateUrl: 'tpl/hardware-list.html',
} )

export class HardwareListComponent implements OnInit, OnChanges {

	loading: Boolean = false;
	error: String;
	hardware: Hardware[] = [];
	@Input() parent?: Hardware;

	constructor(
		private _router: Router,
		private _hardwareService: HardwareService
	) {
	};

	ngOnChanges( changes_: SimpleChanges ) {
		this.getHardware();
	};

	ngOnInit() {
		if ( ! this.loading ) {
			this.getHardware();
		}
	};

	getHardware() {
		var me = this;
		me.loading = true;
		this._hardwareService.getHardwareList( me.parent )
			.subscribe(
				function( hardware_: Hardware[] ) {
					me.loading = false;
					me.hardware = hardware_;
				},
				function( error_: String ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

	selectHardware( hardware_: Hardware ) {
		this._router.navigate( [ '/hardware', hardware_.id ] );
	};

	addHardware( type_: string ) {
		var me = this;
		me.loading = true;
		this._hardwareService.addHardware( type_ )
			.subscribe(
				function( hardware_: Hardware ) {
					me.loading = false;
					me._router.navigate( [ '/hardware/' + hardware_.id ] );
				},
				function( error_: string ) {
					me.loading = false;
					me.error = error_;
				}
			)
		;
	};

}
