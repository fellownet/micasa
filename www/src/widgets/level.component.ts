import { Component, Input, OnInit } from '@angular/core';
import { Device, DevicesService }   from '../devices/devices.service';

declare var Highcharts: any;

@Component( {
	selector: 'levelwidget',
	templateUrl: 'tpl/widgets/level.html'
} )

export class WidgetLevelComponent implements OnInit {

	@Input( 'device' ) public device: Device;
	public error: string;

	private _chart: any;

	constructor(
		private _devicesService: DevicesService
	) {
	};

	ngOnInit() {
		this.getData();
	};

	getData() {
		var me = this;
		me._devicesService.getData( this.device )
			.subscribe(
				function( data_: any[] ) {
					var data: Array<Array<any>> = [];
					for ( var i = 0; i < data_.length; i++ ) {
						data.push( [ data_[i].timestamp * 1000, parseFloat( data_[i].value ) ] );
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
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;

	}
}
