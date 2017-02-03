import { Component, Input, OnInit } from '@angular/core';
import { Widget }                   from '../screens.service';
import { Device, DevicesService }   from '../../devices/devices.service';

declare var Highcharts: any;

@Component( {
	selector   : 'counterwidget',
	templateUrl: 'tpl/widgets/counter.html'
} )

export class WidgetCounterComponent implements OnInit {

	private static _elementId = 0;

	@Input( 'widgetConfig' ) public widget: Widget;

	public error: string;
	public elementId: number;

	constructor(
		private _devicesService: DevicesService
	) {
		++WidgetCounterComponent._elementId;
		this.elementId = WidgetCounterComponent._elementId;
	};

	ngOnInit() {
		this.getData();
	};

	getData() {
		var me = this;
		me._devicesService.getData( this.widget.device.id )
			.subscribe(
				function( data_: any[] ) {
					var data: Array<Array<any>> = [];
					for ( var i = 0; i < data_.length; i++ ) {
						data.push( [ data_[i].timestamp * 1000, parseFloat( data_[i].value ) ] );
					}

					Highcharts.stockChart( 'counter_chart_target_' + me.elementId, {
						chart: {
							type: 'column'
						},
						series: [ {
							name: me.widget.device.name,
							data: data,
							yAxis: 0,
							tooltip: {
								valueSuffix: ' ' + me.widget.device.unit
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
