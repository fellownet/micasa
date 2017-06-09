import {
	Component,
	Input,
	Output,
	OnInit,
	OnDestroy,
	OnChanges,
	AfterViewInit,
	ViewChild,
	ElementRef,
	EventEmitter
}                          from '@angular/core';
import { Observable }      from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import {
	Screen,
	Widget,
	Source
}                          from '../screens.service';
import {
	Device,
	DevicesService
}                          from '../../devices/devices.service';
import { WidgetComponent } from '../widget.component';

declare var require: any;
const Highcharts = require( 'highcharts/highcharts.src.js' );

enum State {
	CREATED          = (1 << 0),
	DATA_RECEIVED    = (1 << 1),
	DEVICES_RECEIVED = (1 << 2),
	VIEW_READY       = (1 << 3)
};

@Component( {
	selector: 'chartwidget',
	templateUrl: 'tpl/widgets/chart.html'
} )

export class WidgetChartComponent implements OnInit, AfterViewInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) public sourceDevices: Device[];

	@Output() onAction = new EventEmitter<string>();

	@ViewChild( 'chartTarget' ) private _chartTarget: ElementRef;

	private _state: BehaviorSubject<number> = new BehaviorSubject( State.CREATED );
	private _active: boolean = true;
	private _chart: any;

	private _colors: any = {
		aqua  : '#00c0ef',
		red   : '#dd4b39',
		yellow: '#f39c12',
		blue  : '#0073b7',
		green : '#00a65a',
		teal  : '#39cccc',
		olive : '#3d9970',
		orange: '#ff851b',
		purple: '#605ca8',
		gray  : '#AAAAAA'
	}

	public editing: boolean = false;
	public title: string;
	public device: Device; // the first source
	public devices: Device[]; // used in the device dropdown while editing
	public data: any = {}; // key = device_id, value = [Device,any[]] tuple

	public constructor(
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		var me = this;

		me.device = me.sourceDevices[me.widget.sources[0].device_id];
		me.title = me.widget.name;

		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			},
			lang: {
				thousandsSep: ''
			}
		} );

		me._devicesService.getDevices()
			.do( function() {
				me._state.next( me._state.getValue() | State.DEVICES_RECEIVED );
			} )
			.subscribe( devices_ => me.devices = devices_ )
		;

		// Render the chart when there's data received *and* thew view is ready. The data received state can happen
		// more than once, so the chart needs to be destroyed first.
		me._state.subscribe( function( state_: number ) {
			if ( ( state_ & ( State.DATA_RECEIVED | State.DEVICES_RECEIVED | State.VIEW_READY ) ) == ( State.DATA_RECEIVED | State.DEVICES_RECEIVED | State.VIEW_READY ) ) {
				if ( !! me._chart ) {
					me._chart.destroy();
				}

				let animationDuration: number = 300;
				let config: any = {
					chart: {
						style: {
							fontFamily: 'helvetica, arial, sans-serif'
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
										radius: 6,
										lineWidth: 2
									}
								}
							}
						}
					}
				};

				let yAxis: number = -1;
				let lastUnit:string = null;
				let serie:number = -1;
				for ( let source of me.widget.sources ) {

					let device: Device = me.data[source.device_id][0];
					let data: any[] = me.data[source.device_id][1];

					// If the device type is 'level' or 'counter', series are added. Switch and text devices are added
					// as plotlines instead.
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

						let range: boolean = (
							data.length > 0
							&& source.properties.range
							&& 'minimum' in data[0]
							&& 'maximum' in data[0]
						);

						config.series[++serie] = {
							data: [],
							visible: !source.properties.hidden,
							name: device.name,
							color: me._colors[source.properties.color || 'aqua'],
							yAxis: yAxis,
							type: device.type == 'level' || [ 'month', 'year' ].indexOf( me.widget.interval ) > -1 ? 'spline' : 'column',
							tooltip: {
								valueSuffix: ' ' + device.unit
							},
							events: {
								hide: function() {
									source.properties.hidden = true;
									if ( range ) {
										this.chart.series[1 + this.chart.series.indexOf( this )].hide();
									}
									if ( !! me.screen.id ) {
										me.save( false );
									}
								},
								show: function() {
									source.properties.hidden = false;
									if ( range ) {
										this.chart.series[1 + this.chart.series.indexOf( this )].show();
									}
									if ( !! me.screen.id ) {
										me.save( false );
									}
								}
							}
						};
						for ( let point of data ) {
							config.series[serie].data.push( [ point.timestamp * 1000, point.value ] );
						}
						if ( device.type == 'counter' ) {
							switch( me.widget.interval ) {
								case 'day':
									config.series[serie].pointRange = 3600 * 1000;
									break;
								case 'week':
									config.series[serie].pointRange = 24 * 3600 * 1000;
									break;
							}
							config.series[serie].zIndex = 10;
						} else {
							config.series[serie].zIndex = 20;
						}

						// Add range.
						if ( range ) {
							config.series[++serie] = {
								data: [],
								color: me._colors[source.properties.color || 'aqua'],
								yAxis: yAxis,
								type: 'areasplinerange',
								showInLegend: false,
								zIndex: 1,
								animation: {
									duration: animationDuration
								},
								lineWidth: 0,
								fillOpacity: 0.13,
								enableMouseTracking: false,
								marker: {
									enabled: false
								}
							};
							for ( let point of data ) {
								config.series[serie].data.push( [ point.timestamp * 1000, point.minimum, point.maximum ] );
							}
						}

						// Add trendline.
						if (
							data.length > 2
							&& source.properties.trendline
						) {
							var ii = 0, x, y, x0, x1, y0, y1, dx,
								m = 0, b = 0, cs, ns,
								n = data.length, Sx = 0, Sy = 0, Sxy = 0, Sx2 = 0, S2x = 0;

							// Do math stuff.
							for (ii; ii < data.length; ii++) {
								x = data[ii].timestamp;
								y = data[ii].value;
								Sx += x;
								Sy += y;
								Sxy += (x * y);
								Sx2 += (x * x);
							}

							// Calculate slope and intercept.
							m = (n * Sx2 - S2x) != 0 ? (n * Sxy - Sx * Sy) / (n * Sx2 - Sx * Sx) : 0;
							b = (Sy - m * Sx) / n;

							// Calculate minimal coordinates to draw the trendline.
							dx = 0;
							x0 = data[0].timestamp - dx;
							y0 = m * x0 + b;
							x1 = data[ii - 1].timestamp + dx;
							y1 = m * x1 + b;

							// Add series.
							config.series[++serie] = {
								data: [
									[ x0 * 1000, y0 ],
									[ x1 * 1000, y1 ]
								],
								name: 'trendline',
								color: me._colors[source.properties.trendline_color || 'red'],
								yAxis: yAxis,
								type: 'line',
								zIndex: 2,
								animation: {
									duration: animationDuration * 4
								},
								lineWidth: 2,
								dashStyle: 'LongDash',
								enableMouseTracking: false,
								marker: {
									enabled: false
								}
							};
						}

					} else if (
						device.type == 'switch'
						|| device.type == 'text'
					) {
						for ( let point of data ) {
							config.xAxis.plotLines.push( {
								color: me._colors[source.properties.color || 'aqua'],
								value: point.timestamp * 1000,
								width: 2,
								label: {
									text: point.value,
									style: {
										color: me._colors[source.properties.color || 'aqua'],
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
					if ( !! me._chartTarget ) {
						me._chart = Highcharts.chart( me._chartTarget.nativeElement, config );
					}
				}, 1 );
			}
		} );
	};

	public ngOnChanges() {
		var me = this;
		me.device = me.sourceDevices[me.widget.sources[0].device_id];
		let observables: Observable<[Device,any[]]>[] = [];
		for ( let source of me.widget.sources ) {
			let device: Device = me.sourceDevices[source.device_id];
			switch( device.type ) {
				case 'switch':
				case 'text':
					observables.push(
						me._devicesService.getData( device.id, {
							range: me.widget.range,
							interval: me.widget.interval
						} )
						.map( function( data_: any ) {
							return [ device, data_ ];
						} )
					);
					break;
				
				case 'level':
					observables.push(
						me._devicesService.getData( device.id, {
							group:
								[ '5min', '5min', 'hour', 'day', 'day' ][
									[ 'hour', 'day', 'week', 'month', 'year' ].indexOf( me.widget.interval )
								],
							range: me.widget.range,
							interval: me.widget.interval
						} )
						.map( function( data_: any ) {
							return [ device, data_ ];
						} )
					);
					break;

				case 'counter':
					observables.push(
						me._devicesService.getData( device.id, {
							group:
								[ 'hour', 'hour', 'day', 'day', 'day' ][
									[ 'hour', 'day', 'week', 'month', 'year' ].indexOf( me.widget.interval )
								],
							range: me.widget.range,
							interval: me.widget.interval
						} )
						.map( function( data_: any ) {
							return [ device, data_ ];
						} )
					);
					break;
			}
		}
		if ( observables.length > 0 ) {
			Observable
				.forkJoin( observables )
				.subscribe( function( data_: [Device,any[]][] ) {
					for ( let data of data_ ) {
						me.data[data[0].id] = [data[0], data[1]];
					}
					me._state.next( me._state.getValue() | State.DATA_RECEIVED );
				} )
			;
		}
	};

	public ngAfterViewInit() {
		this._state.next( this._state.getValue() | State.VIEW_READY );
	};

	public ngOnDestroy() {
		this._active = false;
		this._state.complete();
	};

	public addSource( device_id_: number ) {
		var me = this;
		me._devicesService.getDevice( device_id_ )
			.subscribe( function( device_: Device ) {
				me.data[device_id_] = [device_,[]];
				let source: Source = {
					device_id: +device_id_,
					properties: {
						color: 'blue'
					}
				};
				me.widget.sources.push( source );
			} )
		;
	};

	public removeSource( source_: Source ) {
		let index: number = this.widget.sources.indexOf( source_ );
		if ( index > -1 ) {
			this.widget.sources.splice( index, 1 );
		}
	};

	public save( reload_: boolean = true ) {
		this.onAction.emit( 'save' );
		this.title = this.widget.name;
		this.editing = false;
		if ( reload_ ) {
			this.onAction.emit( 'reload' );
		}
	};

	public delete() {
		this.onAction.emit( 'delete' );
	};

	public reload() {
		this.onAction.emit( 'reload' );
	};

	public changeType() {
		this.onAction.emit( 'type_change' );
	};


}
