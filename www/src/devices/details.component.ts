import {
	Component,
	OnInit,
	AfterViewInit
}                         from '@angular/core';
import { ActivatedRoute } from '@angular/router';

import { Device }         from './devices.service';
import { Hardware }       from '../hardware/hardware.service';
import { Script }         from '../scripts/scripts.service';

declare var Highcharts: any;

@Component( {
	templateUrl: 'tpl/device-details.html'
} )

export class DeviceDetailsComponent implements OnInit, AfterViewInit {

	public loading: boolean = false;
	public error: String;
	public device: Device;
	public data: any[];

	public hardware?: Hardware;
	public script?: Script;

	public constructor(
		private _route: ActivatedRoute
	) {
	};

	public ngOnInit() {
		var me = this;
		this._route.data.subscribe( function( data_: any ) {
			if ( 'hardware' in data_ ) {
				me.hardware = data_.hardware;
			}
			if ( 'script' in data_ ) {
				me.script = data_.script;
			}
			me.device = data_.device;
			me.data = data_.data;
		} );
	};

	public ngAfterViewInit() {
		var me = this;
		if ( me.device.type == "counter" ) {
			var data: Array<Array<any>> = [];
			for ( var i = 0; i < me.data.length; i++ ) {
				data.push( [ me.data[i].timestamp * 1000, parseFloat( me.data[i].value ) ] );
			}

			Highcharts.stockChart( 'counter_chart_target', {
				chart: {
					type: 'column'
				},
				series: [ {
					name: me.device.name,
					data: data,
					yAxis: 0,
					tooltip: {
						valueSuffix: ' ' + me.device.unit
					}
				} ]
			} );
		} else if ( me.device.type == "level" ) {
			var data: Array<Array<any>> = [];
			for ( var i = 0; i < me.data.length; i++ ) {
				data.push( [ me.data[i].timestamp * 1000, parseFloat( me.data[i].value ) ] );
			}

			Highcharts.stockChart( 'level_chart_target', {
				chart: {
					type: 'spline'
				},
				series: [ {
					name: me.device.name,
					data: data,
					yAxis: 0,
					tooltip: {
						valueSuffix: ' ' + me.device.unit
					}
				} ]
			} );
		}
	};
}
