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
	Widget,
	Source
}                          from '../screens.service';
import {
	Device,
	DevicesService
}                          from '../../devices/devices.service';
import { SessionService }  from '../../session/session.service';
import { WidgetComponent } from '../widget.component';

declare var require: any;
const Highcharts = require( 'highcharts/highcharts.src.js' );

enum State {
	CREATED = 0,
	DATA_RECEIVED = 1,
	VIEW_READY = 2
};

@Component( {
	selector: 'chartwidget',
	templateUrl: 'tpl/widgets/chart.html'
} )

export class WidgetChartComponent implements OnInit, AfterViewInit, OnChanges, OnDestroy {

	@Input( 'widget' ) public widget: Widget;
	@Input( 'devices' ) private _devices: Observable<Device[]>;
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

	public loading: boolean = true;
	public devices: Observable<Device[]>; // used in the device dropdown while editing
	public data: any = {}; // key = device_id, value = [Device,any[]] tuple

	public constructor(
		private _devicesService: DevicesService,
		private _sessionService: SessionService
	) {
	};

	public ngOnInit() {
		var me = this;

		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			}
		} );

		me.devices = me._devicesService.getDevices();

		// Render the chart when there's data received *and* thew view is ready. The data received state can happen
		// more than once, so the chart needs to be destroyed first.
		me._state.subscribe( function( state_: number ) {
			if ( ( state_ & ( State.DATA_RECEIVED | State.VIEW_READY ) ) == ( State.DATA_RECEIVED | State.VIEW_READY ) ) {
				if ( !!me._chart ) {
					me._chart.destroy();
				}

				let animationDuration: number = 300;
				let config: any = {
					chart: {
						animation: {
							duration: animationDuration
						},
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
				for ( let source of me.widget.sources ) {

					let device: Device = me.data[source.device_id][0];
					let data: any[] = me.data[source.device_id][1];

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
							color: me._colors[source.properties.color || 'aqua'],
							yAxis: yAxis,
							type: device.type == 'level' || [ 'month', 'year' ].indexOf( me.widget.interval ) > -1 ? 'spline' : 'column',
							tooltip: {
								valueSuffix: ' ' + device.unit
							},
							events: {
								hide: function() {
									source.properties.hidden = true;
									//me.parent.save( false );
								},
								show: function() {
									source.properties.hidden = false;
									//me.parent.save( false );
								}
							}
						};
						for ( let point of data ) {
							config.series[serie].data.push( [ point.timestamp * 1000, parseFloat( point.value ) ] );
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
							config.series[serie].zIndex = 1;
						} else {
							config.series[serie].zIndex = 2;
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
					if ( !!me._chartTarget ) {
						me._chart = Highcharts.chart( me._chartTarget.nativeElement, config );
					}
				}, 1 );
			}
		} );

		// Listen for events broadcasted from the session service.
		me._sessionService.events
			.takeWhile( () => me._active )
			.subscribe( function( event_: any ) {
				console.log( 'got an event in screen component' );
				console.log( event_ );
			} )
		;
	};

	public ngOnChanges() {
		var me = this;
		me.loading = true;
		me._devices.subscribe( function( devices_: Device[] ) {
			let observables: Observable<[Device,any[]]>[] = [];
			for ( let device of devices_ ) {
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
									[ '5min', '5min', 'day', 'day', 'day' ][
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
						me.loading = false;
						me._state.next( me._state.getValue() | State.DATA_RECEIVED );
					} )
				;
			}
		} );
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

	public save() {
		this.onAction.emit( 'save' );
		this.parent.editing = false;
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
