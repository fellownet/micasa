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
	EventEmitter,
	NgZone
}                          from '@angular/core';
import {
	Router
}                          from '@angular/router'; 
import { Observable }      from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import {
	Screen,
	Widget,
	Source,
	SourceData
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
	VIEW_READY       = (1 << 2)
};

@Component( {
	selector: 'chartwidget',
	templateUrl: 'tpl/widgets/chart.html'
} )

export class WidgetChartComponent implements OnInit, AfterViewInit, OnChanges, OnDestroy {

	@Input( 'screen' ) public screen: Screen;
	@Input( 'widget' ) public widget: Widget;
	@Input( 'data' ) public data: SourceData[];
	@Input( 'parent' ) public parent: WidgetComponent;

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

	public invalid: boolean = false;
	public title: string;
	public devices: Observable<{ id: number, name: string, type: string }[]>;

	public constructor(
		private _router: Router,
		private _zone: NgZone,
		private _devicesService: DevicesService
	) {
	};

	public ngOnInit() {
		this.title = this.widget.name;
		this.devices = this._devicesService.getDevices( { enabled: 1 } );

		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			},
			lang: {
				thousandsSep: ''
			}
		} );

		// Render the chart when there's data received *and* thew view is ready. The data received state can happen
		// more than once, so the chart needs to be destroyed first.
		this._state.subscribe( state_ => {
			this._zone.runOutsideAngular( () => {
				if ( ( state_ & ( State.DATA_RECEIVED | State.VIEW_READY ) ) == ( State.DATA_RECEIVED | State.VIEW_READY ) ) {
					if ( !! this._chart ) {
						this._chart.destroy();
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

					// TODO sort sources by device unit

					this.widget.sources.forEach( ( source_, i_ ) => {
						let device: Device = this.data[i_].device;
						let data: any[] = this.data[i_].data;

						// Skip sources with empty data.
						if ( data.length == 0 ) {
							return true;
						}

						if (
							this.data[i_].config.group == 'day'
							&& this.data[i_].data.length <= 12
						) {
							config.xAxis.dateTimeLabelFormats = {
								day: '%a'
							};
						}

						// If the device type is 'level' or 'counter', series are added. Switch and text devices are added
						// as plotlines instead.
						if (
							device.type == 'counter'
							|| device.type == 'level'
						) {
							if ( device.unit != lastUnit ) {
								if ( ++yAxis > 1 ) {
									return false;
								}
								lastUnit = device.unit;
								config.yAxis[yAxis].title = {
									text: device.subtype + ( lastUnit.length > 0 ? ' / ' + lastUnit : '' )
								};
							}

							let range: boolean = (
								source_.properties.range
								&& 'minimum' in data[0]
								&& 'maximum' in data[0]
							);

							config.series[++serie] = {
								data: [],
								visible: ! source_.properties.hidden,
								name: device.name,
								color: this._colors[source_.properties.color || 'aqua'],
								yAxis: yAxis,
								type: device.type == 'level' || [ 'month', 'year' ].indexOf( this.data[i_].config.interval ) > -1 ? 'spline' : 'column',
								dataLabels: {
									enabled: data.length <= 12,
									crop: false,
									overflow: 'none',
									style: {
										fontSize: 10,
										fontWeight: 'normal'
									}
								},
								zIndex: device.type == 'counter' ? 10 : 20,
								tooltip: {
									valueSuffix: ' ' + device.unit
								},
								cursor: 'pointer',
								events: {
									click: ( event_: any ) => {
										this._router.navigate( [ '/devices', device.id ] );
									},
									hide: ( event_: any ) => {
										let serie: any = event_.target;
										source_.properties.hidden = true;
										if ( range ) {
											serie.chart.series[1 + serie.chart.series.indexOf( serie )].hide();
										}
										if ( !! this.screen.id ) {
											this.save( false );
										}
									},
									show: ( event_: any ) => {
										let serie: any = event_.target;
										source_.properties.hidden = false;
										if ( range ) {
											serie.chart.series[1 + serie.chart.series.indexOf( serie )].show();
										}
										if ( !! this.screen.id ) {
											this.save( false );
										}
									}
								}
							};

							let offset: number = 0;
							switch( this.data[i_].config.group ) {
								case 'hour':
									config.series[serie].pointRange = 3600 * 1000;
									config.xAxis.minTickinterval = 3600 * 1000;
									break;
								case 'day':
									config.series[serie].pointRange = 24 * 3600 * 1000;
									config.xAxis.minTickinterval = 24 * 3600 * 1000;
									// Substract the hour part (in the local timezone) from the timestamp to make sure the
									// ticks appear in the center of the pointrange.
									offset = ( ( 12 * 60 ) - new Date().getTimezoneOffset() ) * 60;
									break;
							}

							// Add the data to the serie.
							config.series[serie].data = data.map( point_ => ( { x: ( point_.timestamp - offset ) * 1000, y: point_.value } ) );

							// Add range.
							if ( range ) {
								config.series[++serie] = {
									data: [],
									color: this._colors[source_.properties.color || 'aqua'],
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
									config.series[serie].data.push( [ ( point.timestamp - offset ) * 1000, point.minimum, point.maximum ] );
								}
							}

							// Add trendline.
							if (
								data.length > 2
								&& source_.properties.trendline
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
										[ ( x0 - offset ) * 1000, y0 ],
										[ ( x1 - offset ) * 1000, y1 ]
									],
									name: 'trendline',
									color: this._colors[source_.properties.trendline_color || 'red'],
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
									color: this._colors[source_.properties.color || 'aqua'],
									value: point.timestamp * 1000,
									width: 2,
									label: {
										text: point.value,
										style: {
											color: this._colors[source_.properties.color || 'aqua'],
											fontSize: 10,
											fontWeight: 'bold'
										}
									}
								} );
							}
						}

						return true;
					} );

					// Skip a render pass before drawing the chart.
					setTimeout( () => {
						if ( !! this._chartTarget ) {
							this._chart = Highcharts.chart( this._chartTarget.nativeElement, config );
						}
					}, 1 );
				}
			} ); // end of runOutsideAngular
		} );

		// Instead of listening for broadcasted data for devices we're going to refresh the chart every 5 minutes.
		Observable.interval( 1000 * 60 * 5 )
			.takeWhile( () => this._active )
			.filter( () => ! this.parent.editing && !! this._chart )
			.subscribe( () => {
				this.onAction.emit( 'reload' );
			} )
		;
	};

	public ngOnChanges() {
		// We're doing some housekeeping here. Instead of invalidating the entire chart if a device is removed, the
		// invalid source is removed. If there are no valid sources left, then the widget is marked invalid.
		for ( var j = this.widget.sources.length; j--; ) {
			let source: Source = this.widget.sources[j];
			if (
				! this.data[j].data
				|| ! this.data[j].device
			) {
				this.widget.sources.splice( j, 1 );
			}
		}
		if ( this.widget.sources.length > 0 ) {
			this.invalid = false;
			this._state.next( this._state.getValue() | State.DATA_RECEIVED );
		} else {
			if ( !! this._chart ) {
				this._chart.destroy();
				delete this._chart;
			}
			this.invalid = true;
		}
	};

	public ngAfterViewInit() {
		this._state.next( this._state.getValue() | State.VIEW_READY );
	};

	public ngOnDestroy() {
		this._active = false;
		this._state.complete();
	};

	public addSource( device_id_: string ) {
		let device_id: number = parseInt( device_id_ );
		if ( device_id > 0 ) {
			this._devicesService.getDevice( device_id )
				.subscribe( device_ => {
					this.widget.sources.push( {
						device_id: +device_id_,
						properties: {
							color: 'blue'
						}
					} );
					this.data.push( {
						device: device_,
						data: [],
						config: {}
					} );
				} )
			;
		}
	};

	public removeSource( source_: Source ) {
		let index: number = this.widget.sources.indexOf( source_ );
		if ( index > -1 ) {
			this.widget.sources.splice( index, 1 );
			this.data.splice( index, 1 );
		}
	};

	public save( reload_: boolean = true ) {
		this.onAction.emit( 'save' );
		this.title = this.widget.name;
		this.parent.editing = false;
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
