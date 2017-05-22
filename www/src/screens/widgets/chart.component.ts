import {
	Component,
	Input,
	OnInit,
	AfterViewInit
}                       from '@angular/core';
import { Observable }   from 'rxjs/Observable';

import {
	WidgetComponent
}                       from '../widget.component';
import {
	Device,
	DevicesService
}                       from '../../devices/devices.service'
import {
	Source
}                       from '../screens.service';

declare var require: any;
const Highcharts = require( 'highcharts/highcharts.src.js' );

@Component( {
	selector: 'chartwidget',
	templateUrl: 'tpl/widgets/chart.html'
} )

export class WidgetChartComponent implements OnInit, AfterViewInit {

	private static _elementId = 0;
	public elementId: number;
	private _chart: any;
	private _colors: string[] = [ '#3c8dbc', '#c0392d', '#f1c50d', '#28ae61', '#8d44ad', '#7e8c8d' ];

	public editing: boolean = false;
	public devices: Observable<Device[]>;

	@Input( 'widget' ) public parent: WidgetComponent;
	@Input( 'placeholder' ) public placeholder: boolean;

	public constructor(
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		if ( this.placeholder ) {
			return;
		}

		++WidgetChartComponent._elementId;
		this.elementId = WidgetChartComponent._elementId;

		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			}
		} );

		this.devices = this._devicesService.getDevices();
	};

	public ngAfterViewInit() {
		if ( this.placeholder ) {
			return;
		}

		var me = this;

		let animationDuration: number = 300;
		let config: any = {
			chart: {
				animation: {
					duration: animationDuration
				},
				zoomType: 'x',
				alignTicks: true,
				resetZoomButton: {
					theme: {
						display: 'none'
					}
				}
			},
			title: false,
			credits: false,
			series: [],
			legend: {
				enabled: true,
				itemStyle: {
					fontWeight: 'normal'
				}
			},
			xAxis: {
				type: 'datetime',
				plotLines: [],
				title: {
					text: null
				}
			},
			yAxis: [ {}, { opposite: true, title: { text: '' } } ],
			plotOptions: {
				column: {
					animation: {
						duration: animationDuration
					},
					pointPadding: 0.05,
					groupPadding: 0.1
				},
				spline: {
					animation: {
						duration: animationDuration
					},
					lineWidth: 2,
					states: {
						hover: {
							lineWidth: 2,
							halo: false
						}
					},
					marker: {
						enabled: false,
						states: {
							hover: {
								enabled: true,
								symbol: 'circle',
								radius: 6,
								lineWidth: 2
							}
						}
					}
				}
			}
		};

		// Add day series.
		let yAxis: number = -1;
		let lastUnit:string = null;
		let serie:number = -1;
		let color:number = -1;
		for ( let source of me.parent.widget.sources ) {
			let device: Device = me.parent.devices[source.device_id];
			let data: any[] = me.parent.data[source.device_id];

			// If the device type is 'level' or 'counter', series are added. Switch and text devices are added as
			// plotlines instead.
			if (
				device.type == 'counter'
				|| device.type == 'level'
			) {
				if ( device.unit != lastUnit ) {
					lastUnit = device.unit;
					yAxis++;
					config.yAxis[yAxis].title = {
						text: device.subtype + ( lastUnit.length > 0 ? ' / ' + lastUnit : '' )
					};
				}
				if ( yAxis > 1 ) {
					break;
				}

				config.series[++serie] = {
					data: [],
					visible: !source.properties.hidden,
					name: device.name,
					color: me._colors[++color],
					yAxis: yAxis,
					type: device.type == 'level' || [ 'month', 'year' ].indexOf( me.parent.widget.interval ) > -1 ? 'spline' : 'column',
					tooltip: {
						valueSuffix: ' ' + device.unit
					},
					events: {
						hide: function() {
							source.properties.hidden = true;
							me.parent.save( false );
						},
						show: function() {
							source.properties.hidden = false;
							me.parent.save( false );
						}
					}
				};
				for ( let point of data ) {
					config.series[serie].data.push( [ point.timestamp * 1000, parseFloat( point.value ) ] );
				}
				if ( device.type == 'counter' ) {
					switch( me.parent.widget.interval ) {
						case 'day':
							config.series[serie].pointRange = 3600 * 1000;
							break;
						case 'week':
							config.series[serie].pointRange = 24 * 3600 * 1000;
							break;
					}
					config.series[serie].zIndex = 1;
				} else {
					config.series[serie].zIndex = 2;
				}
			} else if (
				device.type == 'switch'
				|| device.type == 'text'
			) {
				++color;
				for ( let point of data ) {
					config.xAxis.plotLines.push( {
						color: me._colors[color],
						value: point.timestamp * 1000,
						width: 2,
						label: {
							text: point.value,
							style: {
								color: me._colors[color],
								fontSize: 10,
								fontWeight: 'bold'
							}
						}
					} );
				}
			}
		}

		// Skip a render pass before drawing the chart.
		setTimeout( function() {
			me._chart = Highcharts.chart( 'chart_target_' + me.elementId, config );
		}, 1 );
	};

	public addSource( device_id_: number ) {
		var me = this;
		me._devicesService.getDevice( device_id_ )
			.subscribe( function( device_: Device ) {
				me.parent.devices[device_id_] = device_;
				let source: Source = {
					device_id: device_id_,
					properties: {}
				};
				me.parent.widget.sources.push( source );
			} )
		;
	};

	public removeSource( source_: Source ) {
		let index: number = this.parent.widget.sources.indexOf( source_ );
		if ( index > -1 ) {
			this.parent.widget.sources.splice( index, 1 );
		}
	};

}
