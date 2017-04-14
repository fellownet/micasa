import {
	Component,
	Input,
	OnInit,
	AfterViewInit,
	OnDestroy
}                       from '@angular/core';

import {
	Screen,
	Widget
}                       from '../screens.service';
import {
	Device,
	DevicesService
}                       from '../../devices/devices.service';
import {
	Router,
}                       from '@angular/router';
import { Subscription } from 'rxjs/Subscription';

declare var Highcharts: any;

@Component( {
	selector: 'counterwidget',
	templateUrl: 'tpl/widgets/counter.html'
} )

export class WidgetCounterComponent implements OnInit, AfterViewInit, OnDestroy {

	private static _elementId = 0;

	private _chart: any;
	private _subscription: Subscription;

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'device' ) public device: Device;

	public error: string;
	public elementId: number;
	public loading: boolean = true;
	public editing: boolean = false;

	public constructor(
		private _router: Router,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		++WidgetCounterComponent._elementId;
		this.elementId = WidgetCounterComponent._elementId;
	};

	public ngOnDestroy() {
		if ( this._subscription ) {
			this._subscription.unsubscribe();
		}
	};

	public ngAfterViewInit() {
		var me = this;
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
			}
		} );
		me._loadData();
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
		this._loadData();
	};

	public delete() {
		alert( 'delete' );
	};

	public edit() {
		this.editing = true;
	};

	private _loadData(): void {
		var me = this;
		me.loading = true;
		me._subscription = me._devicesService.getData( this.device.id, { group: 'hour', range: 1, interval: 'day' } )
			.subscribe(
				function( data_: any[] ) {
					me.loading = false;
					var data: Array<Array<any>> = [];
					for ( var i = 0; i < data_.length; i++ ) {
						data.push( [ data_[i].timestamp * 1000, parseFloat( data_[i].value ) ] );
					}

					// First remove previes series if there is one (for instance when the
					// refresh tool was clicked).
					if ( me._chart.series.length > 0 ) {
						me._chart.series[0].remove( true );
						me._chart.zoomOut();
					}

					// Then add the refreshed series.
					me._chart.addSeries( {
						name: me.device.name,
						data: data,
						tooltip: {
							valueSuffix: ' ' + me.device.unit
						}
					} );
				},
				function( error_: string ) {
					me.error = error_;
				}
			)
		;
	};

}
