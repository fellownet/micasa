import {
	Component,
	Input,
	OnInit
}                  from '@angular/core';
import {
	Router,
}                  from '@angular/router';

import {
	Screen,
	Widget
}                  from '../screens.service';
import {
	Device,
	DevicesService
}                  from '../../devices/devices.service';

declare var Highcharts: any;

@Component( {
	selector: 'counterwidget',
	templateUrl: 'tpl/widgets/counter.html'
} )

export class WidgetCounterComponent implements OnInit {

	private static _elementId = 0;

	private _chart: any;

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'device' ) public device: Device;

	public error: string;
	public elementId: number;

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
		++WidgetCounterComponent._elementId;
		this.elementId = WidgetCounterComponent._elementId;
	};

	public ngOnInit() {
		var me = this;
		me._devicesService.getData( this.device.id, { group: 'hour', range: 1, interval: 'day' } )
			.subscribe(
				function( data_: any[] ) {
					var data: Array<Array<any>> = [];
					for ( var i = 0; i < data_.length; i++ ) {
						data.push( [ data_[i].timestamp * 1000, parseFloat( data_[i].value ) ] );
					}

					me._chart = Highcharts.chart( 'counter_chart_target_' + me.elementId, {
						chart: {
							type: 'column',
							events: {
								click: function() {
									me._router.navigate( [ '/screens', me.screen.id, 'device', me.device.id ] );
								}
							}
						},
						xAxis: {
							title: {
								text: null
							}
						},
						yAxis: {
							title: {
								text: null
							}
						},
						series: [ {
							name: me.device.name,
							data: data,
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
	};

	public setClasses() {
		return {
			'col-xs-12': true,
			'col-sm-12': true,
			'col-md-6': true,
			'col-lg-4': true
		};
	};

	public reload() {
		alert( 'reload' );
	};

	public delete() {
		alert( 'delete' );
	};

	public edit() {
		alert( 'edit' );
	};

}
